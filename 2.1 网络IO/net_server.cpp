```c++
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <unistd.h>

#include <pthread.h>
#include <sys/epoll.h>

#include <string.h>
#define BUFFER_LENGTH 128
#define EVENTS_LENGTH 128

char rbuffer[BUFFER_LENGTH] = {0};
char wbuffer[BUFFER_LENGTH] = {0};

/*epoll 存在共用一个数据缓冲区问题，为了让每一个fd都有自己独立的缓冲区
1.lsitenfd与clientfd的sock_item有没有区别?
2.100万的sock_item如何快速查找？
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

struct reactor{
    int epfd;

};
//线程回调函数
void *routine(void* arg){
    int clientfd = *(int *)arg;
    
    while(1){
        unsigned char buffer[BUFFER_LENGTH] = {0};
        int ret = recv(clientfd, buffer, BUFFER_LENGTH, 0);
        printf("buffer : %s, ret : %d\n", buffer, ret);
        if(ret == 0){
            close(clientfd);
            break;
        }
        ret = send(clientfd, buffer, ret, 0);
    }
    return 0;
}

int main(){

    //fd创建的时候默认是block(阻塞的)
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);  //listenfd 是从3开始一次往后累加分配的，0，1，2分别是stdin,stdout,stderr
    //socket(AF_INET协议地址族, SOCK_STREAM, 0) 接口的固定写法，由于历史遗留原因，除了第二个参数会修改外，其他两个参数基本都不会修改, fd就是一个链接的句柄，即标识号
    if(listenfd == -1) return -1;   //unix api 成功的返回值一般都为0

    struct sockaddr_in servaddr;    //网络通信的地址设置，协议，IP，端口
    servaddr.sin_family = AF_INET;
    //htonl,host to network long， host（点分十进制）转换为 long网络字节序
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);    //INADDR_ANY是"0.0.0.0" 宏定义，addr一般指ipv4的地址
    servaddr.sin_port = htons(9999); //端口

    if(-1 == bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
        return -2;
    }

#if 0   //nonblock（非阻塞fd设置）
    int flag = fcntl(listenfd, F_GETFL, 0); //先get出属性
    flag |= O_NONBLOCK;  //或，位运算,加上新属性，标准写法，不会覆盖之前的属性
    fcntl(listenfd, F_SETFL, flag); //set进去
#endif

    listen(listenfd, 10);

#if 0
    //单一网络连接、网络io
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    //这里的listenfd是阻塞的fd，没有数据连接，进程一直阻塞住，等客户端的连接过来
    int clientfd = accept(listenfd, (struct sockaddr*)&clientaddr, &len);   //accpet接收成功会生成一个新的socketfd当作客户端的fd
    
    unsigned char buffer[BUFFER_LENGTH] = {0};
    int ret = recv(clientfd, buffer, BUFFER_LENGTH, 0);
    if(ret == 0){   //recv返回0，代表客户端连接断开
        close(clientfd);
    }
    printf("buffer : %s, ret : %d\n", buffer, ret);

    ret = send(clientfd, buffer, ret, 0);
    
    //printf("clientfd : %d\n", clientfd);

#elif 0
    //多线程：一请求一线程
    //好处：逻辑简单  缺点：最多1024个客户端连接、多线程
    while(1){
        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);
        int clientfd = accept(listenfd, (struct sockaddr*)&clientaddr, &len);   //只能用于阻塞的fd

        pthread_t threadid;
        pthread_create(&threadid, NULL, routine, &clientfd);
    }

#elif 0
   //select：io多路复用组件，单线程解决多个客户端连接，select(maxfd, rset, wset, err, timeout);
    fd_set rfds, wfds, rset, wset;  //io可读、可写事件，bit位数据结构，这就是select的核心 rfsd/rset可读集合、wfds/wset可写集合,
    FD_ZERO(&rfds);     //清空fd设置，比特位清空
    FD_SET(listenfd, &rfds);

    FD_ZERO(&wfds);

    int maxfd = listenfd;   //最大的fd值
    unsigned char buffer[BUFFER_LENGTH] = {0};
    int ret=0;

    while(1){
        //我们自己操作可读、可写事件用fds，检测可读、可写事件用set
        rset = rfds;
        wset = wfds;
        //监听listenfd，并创建clientfd
        //select实现是个for遍历，maxfd就是循环的最大值，返回值位可读可写的事件总数
        int nready = select(maxfd+1, &rset, &wset, NULL, NULL);  //如果没有可读写事件会阻塞住，有则会修改rset、wset状态
        if(FD_ISSET(listenfd, &rset)){  //判断listenfd在可读事件里有没有，位运算判断 
            printf("listenfd--> \n");
            struct sockaddr_in clientaddr;
            socklen_t len = sizeof(clientaddr);
            int clientfd = accept(listenfd, (struct sockaddr*)&clientaddr, &len);//新增clentfd,accept会把listenfd可读可写事件清空

            FD_SET(clientfd, &rfds);    //设置clientfd可读事件
            if(clientfd > maxfd) maxfd = clientfd;
        }

        //处理clientfd内的数据
        int i=0;
        for(i=listenfd+1; i<=maxfd; i++){   //clientfd都是在listenfd(=3)上累加的,这里的i就是clientfd
            if(FD_ISSET(i, &rset)){ //如果clientfd有读事件
                ret = recv(i, buffer, BUFFER_LENGTH, 0);
                if(ret == 0){
                    close(i);
                    FD_CLR(i, &rfds);   //清空可读事件i
                }else if(ret > 0){
                    printf("buffer : %s, ret : %d\n", buffer, ret);  
                    FD_SET(i, &wfds);   //设置可写事件i 
                }
            }else if(FD_ISSET(i, &wset)){
                ret = send(i, buffer, ret, 0);
                FD_CLR(i, &wfds);
                FD_SET(i, &rfds);
            }
        }
    }

#else 
    //Epoll io多路复用: epoll_create(int)、epoll_ctl(epfd, OPERA, fd, ev)、epoll_wait(epfd, events, evlength, timeout);
    int epfd = epoll_create(1); //参数只要大于0即可，1和100没有区别，看源码

    struct epoll_event ev, events[EVENTS_LENGTH];
    
    //将listenfd添加进epoll
    ev.events = EPOLLIN;    //设置为读事件
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    while(1){
        //timeout:epoll_wait的第四个参数，-1为阻塞等数据，0为立即返回，1000：每1s返回一次
        int nready = epoll_wait(epfd, events, EVENTS_LENGTH, -1); //如果检测到可读可写事件，nready为可读写事件的总数，事件存储在events内
        printf("nready---->: %d\n", nready);

        int i=0;
        for(i=0; i<nready; i++){
            int evfd = events[i].data.fd;   //evfd事件有两种一个是lisentfd=3，另一种是clientfd

            if(evfd == listenfd){   //listenf可读事件触发，即客户端请求连接服务端：connect()->accept()
                struct sockaddr_in clientaddr;
                socklen_t len = sizeof(clientaddr);
                int clientfd = accept(listenfd, (struct sockaddr*)&clientaddr, &len);
                printf("accpet：%d\n", clientfd);
                //将clientfd添加进epoll
                ev.events = EPOLLIN;  //EPOLLIN表示读事件，默认水平触发LT（只要buffer有数据就一直触发，只需要写一个recv即可）
                //ev.events = EPOLLIN | EPOLLET;  //边沿触发ET，需要循环recv "while(recv(...)){}" 直到buffer内无数据可读
                ev.data.fd = clientfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);  //将clientfd加到epoll检测队列中去

            }else if(events[i].events & EPOLLIN){  //clientfd可读事件触发，recv()
                //char rbuffer[BUFFER_LENGTH] = {0};
                int n = recv(evfd, rbuffer, BUFFER_LENGTH, 0);   //水平触发LT，buffer里有数据就一直触发 
                if(n > 0){
                    rbuffer[n]='\0';
                    printf("recv: %s\n", rbuffer);

                    memcpy(wbuffer, rbuffer, BUFFER_LENGTH);

                    ev.events = EPOLLOUT;
                    ev.data.fd = evfd;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, evfd, &ev);  //修改evfd（clientfd）为写事件
                }else if(n == 0){
                    close(evfd);
                }
                /*
                while(recv(evfd, buffer, BUFFER_LENGTH, 0)){      //边沿触发ET,边沿触发使用较多
                    printf("recv: %s\n", buffer);
                    //send(evfd, buffer, sizeof(buffer), 0);
                    memset(buffer,0,sizeof(buffer));
                }*/
            }else if(events[i].events & EPOLLOUT){  //clientfd可写事件触发，send()
                int sent = send(evfd, wbuffer, BUFFER_LENGTH, 0);
                //printf("sent：%d\n", sent);
                
                ev.events = EPOLLIN;
                ev.data.fd = evfd;
                epoll_ctl(epfd, EPOLL_CTL_MOD, evfd, &ev);  //修改evfd（clientfd）为写事件
            }
        }
    }

#endif

    return 0;
}
```
