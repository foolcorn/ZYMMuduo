#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <vector>

inline void ERR_EXIT(const char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

using pollFdList = std::vector<struct pollfd>;

int main(int argc, char *argv[]){
    struct sigaction newAction;
    newAction.sa_handler = SIG_IGN;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_flags = 0;
    //-忽略SIGPIPE信号，可以用sigprocmask直接屏蔽吗？
    sigaction(SIGPIPE,&newAction,NULL);
    //-忽略父子进程信号
    sigaction(SIGCHLD,&newAction,NULL);

    int listenfd;

    //-监听socket，ipv4,TCP,非阻塞，该文件描述符在exec时自动关闭，
    //-在Unix/Linux系统中，在不同的版本中这两者有微小差别.对于BSD,是AF,对于POSIX是PF.
    if((listenfd = socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0))<0){
        ERR_EXIT("socket");
    }

    //-地址
    struct sockaddr_in servaddr;
    //-给这个结构体所有数据全部初始化为0
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //-将5188转换成网络的大端序
    servaddr.sin_port = htons(5188);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//-本机ip地址

    int on = 1;
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0){
        ERR_EXIT("setsockopt");
    }
    if(bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr))){
        ERR_EXIT("bind");
    }

    return 0;



}