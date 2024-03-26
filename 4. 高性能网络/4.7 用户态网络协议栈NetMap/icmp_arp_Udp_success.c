
#define NETMAP_WITH_LIBS        //开启这个宏定义，才能使用相关的结构体及接口

#include <net/netmap_user.h>
#pragma pack(1)         //用一个字节对齐

#define PROTO_IP        0x0800
#define PROTO_UDP       17

#define ETHER_ADDR_LEN  6

//sizeof(struct ether_hdr);
struct  etherhdr{      //以太网头
    unsigned char dst_mac[ETHER_ADDR_LEN];
    unsigned char src_mac[ETHER_ADDR_LEN];
    unsigned short protocol;
};

struct iphdr {         //ip头
    unsigned char version:4,
                hdrlen:4;   //两个4位大小类型的定义方式
    unsigned char tos;
    unsigned short totlen;
    
    unsigned short id;
    unsigned short flag:3,
                offset:13;
    
    unsigned char ttl;
    unsigned char protocol;

    unsigned short check;

    unsigned int sip;
    unsigned int dip;
};

struct udphdr{       //udp头
    unsigned short sport;
    unsigned short dport;

    unsigned short length;
    unsigned short check;
};


struct udppkt{
    
    struct etherhdr eth;
    struct iphdr ip;
    struct udphdr udp;
    
    /*
    unsigned char *payload;         //不能保证连续内存空间
    unsigned char payload[65535];   //可能会越界
    */
   
    unsigned char payload[0];       //零长数组，保证连续空间而且不会越界，这个值的大小我们是可以计算出来
};


//netmap 是个人开源的代码库 适合做调研，不适合做产品
int main(){
    
    struct nm_pkthdr h;
    struct nm_desc *nmr = nm_open("netmap:eth0", NULL, 0, NULL);
    if (nmr == NULL) return -1;

    struct pollfd pfd = {0};

    pfd.fd = nmr->fd;
    pfd.events = POLLIN;

    while(1) {
        int ret = poll(&pfd, 1, -1);
        if( ret < 0 ) continue;

        if(pfd.events & POLLIN){
            unsigned char *stream = nm_nextpkt(nmr, &h);
            struct etherhdr *eh = (struct etherhdr*) stream;
            if(ntohs(eh->protocol == PROTO_IP)){
                struct udppkt *pkt = (struct udppkt *) stream;
                if(pkt->ip.protocol == PROTO_UDP){
                    int length = ntohs(pkt->udp.length);
                    pkt->payload[length-8] = '\0';
                    printf("pkt: %s\n", pkt->payload);
                }
            }
        }
    }
}