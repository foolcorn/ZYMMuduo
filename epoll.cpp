#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <vector>
#include <algorithm>
#include <iostream>

using EventList = std::vector<struct epoll_event>;
using BOOL = int;
const int TRUE = 1;
const int FALSE = 0;

inline void ERR_EXIT(const char*msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

int doepoll(void){
    signal(SIGPIPE,SIG_IGN);
    signal(SIGCHLD,SIG_IGN);

    int idlefd = open("/dev/null",O_RDONLY | O_CLOEXEC);
    int listenfd;
    listenfd = socket("AF_INET",SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
    if(listenfd<0){
        ERR_EXIT("socket");
    }
    BOOL on = TRUE;
    //-设置地址可重用
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    struct sockaddr_in seraddr;
    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons("3741");
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(listenfd,&seraddr,sizeof(seraddr))<0){
        ERR_EXIT("bind");
    }
    if(listen(listenfd,SOMAXCONN)<0){
        ERR_EXIT("listen");
    }

    std::vector<int> clients;//-存所有现存客户端的文件描述符
    int epollfd;
    epollfd = epoll_create1(EPOLL_CLOEXEC);

    struct epoll_event event;
    event.data.fd = listenfd;
    event.events = EPOLLIN;//-默认是水平触发模式
    epoll_ctl(epollfd,EPOLL_CTL_ADD,listenfd,&event);

    EventList events(16);//-返回的活跃事件列表
    struct sockaddr_in peeraddr;
    socklen_t peerlen;
    int connfd;

    int nready;
    while(true){
        nready = epoll_wait(epollfd,events.data(),(int)events.size(),-1);
        if(nready == -1){
            if(errno == EINTR){//-读或写的时候出现中断
                continue;
            }
            ERR_EXIT("epoll_wait");
        }
        if(nready == 0){
            continue;
        }
        if((size_t)nready == events.size()){
            events.resize(events.size()*2);
        }
        for(int i = 0;i<nready;++i){
            if(events[i].data.fd == listenfd){
                peerlen = sizeof(peeraddr);
                connfd = ::accept4(listenfd, reinterpret_cast<sockaddr *>(&peeraddr), &peerlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if(connfd == -1){
                    if(errno == EMFILE){
                        close(idlefd);
                        idlefd = accept(listenfd,NULL,NULL);
                        close(idlefd);
                        idlefd = open("/dev/null",O_RDONLY|O_CLOEXEC);
                        continue;
                    }else{
                        ERR_EXIT("accept4");
                    }
                }

                std::cout << "ip=" <<inet_ntoa(peeraddr.sin_addr)<<
                "port="<<ntohs(peeraddr.sin_port)<<std::endl;
                clients.push_back(connfd);

                event.data.fd = connfd;
                event.events = EPOLLIN;
                epoll_ctl(epollfd,EPOLL_CTL_ADD,connfd,&event);
            }
            else if(events[i].events & EPOLLIN){
                 connfd = event[i].data.fd;
                 if(connfd < 0){
                     continue;
                 }
                 char buf[1024] = {0};
                 int ret = read(connfd,buf,1024);
                 if(ret == -1){
                     ERR_EXIT("read");
                 }
                 if(ret == 0){
                     std::cout<<"client close"<<std::endl;
                     close(connfd);
                     event = events[i];
                     epoll_ctl(epollfd,EPOLL_CTL_DEL,connfd,&event);
                     //-为什么要删除这么多呢
                     clients.erase(std::remove(clients.begin(),clients.end(),connfd),clients.end());
                     continue;
                 }
                 std::cout<<buf;
                 write(connfd,buf,strlen(buf));
            }

        }




    }




}