#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>


#define BUFFER_LENGTH 128
#define EVENTS_LENGTH 128

#define PORT_COUNT  100
#define ITEM_LENGTH 1024

/*epoll 存在共用一个数据缓冲区问题，为了让每一个fd都有自己独立的缓冲区
所以就有了reactor*/
struct sock_item{   //conn_item
    int fd; //clientfd
    char *rbuffer;
    int rlength;
    char *wbuffer;
    int wlength;
    int event;
    //callback
    void (*recv_cb)(int fd, char *buffer, int length);
    void (*send_cb)(int fd, char *buffer, int length);
    void (*accept_cb)(int fd, char *buffer, int length);
};
 
/*动态扩容数组
1.在有序数据存储查找，动态扩容数组的空间和时间复杂度优于红黑树，并且结构简单，实际工作中经常会使用
*/
struct eventblock{
    struct sock_item *items;
    struct eventblock *next;
};

struct reactor{
    int epfd;
    int blkcnt;
    struct eventblock* evblk;
};

int reactor_resize(struct reactor *r){       //新增 eventblock 扩容
    if(r == NULL) return -1;
    struct eventblock *blk = r->evblk;
    while(blk != NULL && blk->next != NULL){   //移动到最后
        blk = blk->next;
    }
    struct sock_item *item = (struct sock_item*)malloc(ITEM_LENGTH * sizeof(struct sock_item));   //新增item数组
    if(item == NULL) return -4;
    memset(item, 0, ITEM_LENGTH * sizeof(struct sock_item));

    printf("---blkcnt---: %d\n", r->blkcnt);
    struct eventblock *block = (struct eventblock*)malloc(sizeof(eventblock));  //新增eventblock模块
    if(block == NULL){
        free(item);
        return -5;
    }
    memset(block, 0, sizeof(eventblock));

    block->items = item;    //新增eventblock赋值
    block->next = NULL;

    if(blk == NULL){
        r->evblk = block;      //连接eventblock模块
    } else {
        blk->next = block;
    }
    r->blkcnt++;
    
    return 0;
}

struct sock_item* reactor_lookup(struct reactor *r, int sockfd){
    if(r == NULL || sockfd <= 0) return NULL;

    int blkidx = sockfd / ITEM_LENGTH;

    //printf("reactor_lookup --> %d\n", r->blkcnt);
    while(blkidx >= r->blkcnt){     //超出边界
        reactor_resize(r);      //扩容
    }
    int i=0;
    struct eventblock *blk = r->evblk;
    while(i++ < blkidx && blk != NULL){
        blk = blk->next;
    }

    return &blk->items[sockfd % ITEM_LENGTH];
}

int init_server(short port){
    //fd创建的时候默认是block(阻塞的)
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);  //listenfd 是从3开始一次往后累加分配的，0，1，2分别是stdin,stdout,stderr
    //socket(AF_INET协议地址族, SOCK_STREAM, 0) 接口的固定写法，由于历史遗留原因，除了第二个参数会修改外，其他两个参数基本都不会修改, fd就是一个链接的句柄，即标识号
    if(listenfd == -1) return -1;   //unix api 成功的返回值一般都为0

    struct sockaddr_in servaddr;    //网络通信的地址设置，协议，IP，端口
    servaddr.sin_family = AF_INET;
    //htonl,host to network long， host（点分十进制）转换为 long网络字节序
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);    //INADDR_ANY是"0.0.0.0" 宏定义，addr一般指ipv4的地址
    servaddr.sin_port = htons(port); //端口

    if(-1 == bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
        return -2;
    }

#if 1   //nonblock（非阻塞fd设置）
    int flag = fcntl(listenfd, F_GETFL, 0); //先get出属性
    flag |= O_NONBLOCK;  //或，位运算,加上新属性，标准写法，不会覆盖之前的属性
    fcntl(listenfd, F_SETFL, flag); //set进去
#endif

    listen(listenfd, 10);

    return listenfd;
}

int is_listenfd(int *fds, int connfd){

    int i=0;
    for(i=0; i<PORT_COUNT; i++){
        if(fds[i] == connfd){
            return 1;
        }
    }
    return 0;
}

int main(){
    struct reactor *r= (struct reactor*)calloc(1, sizeof(struct reactor));
    if(r == NULL){
        return -3;
    }
    //Epoll io多路复用: epoll_create(int)、epoll_ctl(epfd, OPERA, fd, ev)、epoll_wait(epfd, events, evlength, timeout);
    r->epfd = epoll_create(1); //参数只要大于0即可，1和100没有区别，看源码

    struct epoll_event ev, events[EVENTS_LENGTH];
    
    int listenfds[PORT_COUNT] = {0};
    int i=0;
    for(i=0; i<PORT_COUNT; i++){
        listenfds[i] = init_server(9999 + i);
        //将listenfd添加进epoll
        ev.events = EPOLLIN;    //设置为读事件
        ev.data.fd = listenfds[i];
        epoll_ctl(r->epfd, EPOLL_CTL_ADD, listenfds[i], &ev);
    }
    

    while(1){
        //timeout:epoll_wait的第四个参数，-1为阻塞等数据，0为立即返回，1000：每1s返回一次
        int nready = epoll_wait(r->epfd, events, EVENTS_LENGTH, -1); //如果检测到可读可写事件，nready为可读写事件的总数，事件存储在events内
        //printf("nready---->: %d\n", nready);

        int i=0;
        for(i=0; i<nready; i++){
            int eventfd = events[i].data.fd;   //evfd事件有两种一个是lisentfd=3，另一种是clientfd
    //accept
            if(is_listenfd(listenfds, eventfd)){   //listenf可读事件触发，即客户端请求连接服务端：connect()->accept()
                struct sockaddr_in clientaddr;
                socklen_t len = sizeof(clientaddr);
                int connfd = accept(eventfd, (struct sockaddr*)&clientaddr, &len);
                if(connfd == -1) break;
                
                if(connfd % 1000 == 999)
                    printf("clientfd:%d,client_ip:%s\n", connfd, inet_ntoa(clientaddr.sin_addr));
               
#if 1   //nonblock
                int flag = fcntl(connfd, F_GETFL, 0);
                flag |= O_NONBLOCK;
                fcntl(connfd, F_SETFL, flag);
#endif                
                //将connfd添加进epoll
                ev.events = EPOLLIN;  //EPOLLIN表示读事件，默认水平触发LT（只要buffer有数据就一直触发，只需要写一个recv即可）
                //ev.events = EPOLLIN | EPOLLET;  //边沿触发ET，需要循环recv "while(recv(...)){}" 直到buffer内无数据可读
                ev.data.fd = connfd;
                epoll_ctl(r->epfd, EPOLL_CTL_ADD, connfd, &ev);  //将connfd加到epoll检测队列中去
#if 0
                r->items[connfd].fd = connfd;
                r->items[connfd].rbuffer = (char*)calloc(1, BUFFER_LENGTH);
                r->items[connfd].rlength = 0;
                r->items[connfd].wbuffer = (char*)calloc(1, BUFFER_LENGTH);
                r->items[connfd].wlength = 0;
                r->items[connfd].event = EPOLLIN; 
#else
                struct sock_item *item = reactor_lookup(r, connfd);
                item->fd = connfd;
                item->rbuffer = (char*)calloc(1, BUFFER_LENGTH);
                item->rlength = 0;
                item->wbuffer = (char*)calloc(1, BUFFER_LENGTH);
                item->wlength = 0;
#endif    
    //recv
            }else if(events[i].events & EPOLLIN){  //clientfd可读事件触发，recv()
                struct sock_item *item = reactor_lookup(r, eventfd);
                char *rbuffer = item->rbuffer;
                char *wbuffer = item->wbuffer;

                int n = recv(eventfd, rbuffer, BUFFER_LENGTH, 0);   //水平触发LT，buffer里有数据就一直触发 
                if(n > 0){
                    //printf("recv: %s\n", rbuffer);
                    memcpy(wbuffer, rbuffer, BUFFER_LENGTH);

                    ev.events = EPOLLOUT;
                    ev.data.fd = eventfd;
                    epoll_ctl(r->epfd, EPOLL_CTL_MOD, eventfd, &ev);  //修改clientfd为写事件
                    
                }else if(n == 0){       //客户端主动断开
                    free(rbuffer);
                    free(wbuffer);
                    item->fd = 0;
                    close(eventfd);
                }
                /*
                while(recv(clientfd, buffer, BUFFER_LENGTH, 0)){      //边沿触发ET,边沿触发使用较多
                    printf("recv: %s\n", buffer);
                    //send(clientfd, buffer, sizeof(buffer), 0);
                    memset(buffer,0,sizeof(buffer));
                }*/
    //send
            }else if(events[i].events & EPOLLOUT){  //clientfd可写事件触发，send()
                struct sock_item *item = reactor_lookup(r, eventfd);
                char *wbuffer = item->wbuffer;
                int sent = send(eventfd, wbuffer, BUFFER_LENGTH, 0);
                //printf("sent：%d\n", sent);
                
                ev.events = EPOLLIN;
                ev.data.fd = eventfd;
                epoll_ctl(r->epfd, EPOLL_CTL_MOD, eventfd, &ev);  //修改clientfd为写事件
                
            }
        }
    }

    return 0;
}