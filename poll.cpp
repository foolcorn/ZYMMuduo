#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
//#include <sys/stat.h>
//#include <sys/types.h>

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

using PollFdList = std::vector<struct pollfd>;

int main(int argc, char *argv[]){
    struct sigaction newAction;
    newAction.sa_handler = SIG_IGN;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_flags = 0;
    //-忽略SIGPIPE信号，可以用sigprocmask直接屏蔽吗？
    sigaction(SIGPIPE,&newAction,NULL);
    //-忽略父子进程信号
    sigaction(SIGCHLD,&newAction,NULL);

    int listenfd;//-监听socket文件描述符
    //-提前建立一个占位的文件描述符，避免大并发时无文件描述符可用的情况
    int idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);

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
    //-服务端随便选一个端口，将5188转换成网络的大端序
    servaddr.sin_port = htons(5188);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//-本机ip地址,sin_addr是结构体,里面只有一个成员s_addr

    int on = 1;//-这个on其实可以想象成bool on = true;只不过setsockopt是c语言接口，所以没有bool类型
    //-设置地址重复利用（优化timewait，一旦可以立刻复用端口），必须在bind之前设置该地址可以重用
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0){
        ERR_EXIT("setsockopt");
    }
    if(bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr))){
        ERR_EXIT("bind");
    }
    if(listen(listenfd, SOMAXCONN)<0){
        ERR_EXIT("listen");
    }

    //-poll文件描述符
    struct pollfd pfd;
    pfd.fd = listenfd;//-监听事件
    pfd.events = POLLIN;//-关注的是pollin事件（有数据可读）

    PollFdList pollfds;
    pollfds.push_back(pfd);//-第一个套接字总是监听套接字，后面pushback的才是已连接套接字

    int nready;//-等会用来获取有几个活跃事件的变量

    struct sockaddr_in peeraddr;//-这个地址结构用来存对端地址信息
    socklen_t peerlen;
    int connfd;

    while(true){
        //-数组首地址，数组长度，超时时间：-1无限
        nready = poll(pollfds.data(),pollfds.size(),-1);
        if(nready == -1){
            if(errno == EINTR){
                continue;
            }
            ERR_EXIT("poll");
        }
        if(nready == 0){
            continue;
        }
        //-如果是监听套接字有反应,说明有新连接建立
        if(pollfds[0].revents & POLLIN){
            peerlen = sizeof(peeraddr);
            //-返回已连接socket，参数：监听socket，sockaddr指针，socklen_t指针，flag
            connfd = ::accept4(listenfd,(struct sockaddr*)&peeraddr,&peerlen, SOCK_NONBLOCK|SOCK_CLOEXEC);
//            if(connfd == -1){
//                ERR_EXIT("accept4");
//            }
            if(connfd == -1){
                if (errno == EMFILE){
                    close(idlefd);
                    idlefd = accept(listenfd,NULL,NULL);
                    close(idlefd);
                    idlefd = open("/dev/null",O_RDONLY|O_CLOEXEC);
                    continue;
                }
                else{
                    ERR_EXIT("accept4");
                }
            }
            //-把新连接的套接字插入监听数组中
            pfd.fd = connfd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            pollfds.push_back(pfd);
            --nready;//-活跃数-1

            std::cout<<"ip="<<inet_ntoa(peeraddr.sin_addr)<<" port=";
            std::cout<<ntohs(peeraddr.sin_port)<<std::endl;
            //-如果只有监听套接字活跃，就进入下个循环
            if(nready == 0){
                continue;
            }
        }
        //-打印监听数组长度
        std::cout<<pollfds.size() << std::endl;
        std::cout << nready<< std::endl;//-打印活跃数量
        for(auto it = pollfds.begin()+1; it != pollfds.end()&&nready>0; ++it){
            if(it -> revents & POLLIN){
                --nready;
                connfd = it -> fd;
                char buf[1024] = {0};//-buffer数组初始化为0
                int readcount = read(connfd,buf,1024);
                if (readcount == -1)
                    ERR_EXIT("read");
                if( readcount == 0){
                    std::cout<<"client close"<<std::endl;
                    it = pollfds.erase(it);//-erase后会指向下一个元素，但是for循环里面还有一个++it
                    --it;//-用这个抵消
                    close(connfd);
                    continue;
                }
                std::cout<<buf;
                write(connfd,buf,strlen(buf));
            }
        }
    }
    return 0;
}