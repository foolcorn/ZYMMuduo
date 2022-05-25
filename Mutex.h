#ifndef ZYMMUDUO_DEMO1_H
#define ZYMMUDUO_DEMO1_H

#include<unistd.h>
#include<pthread.h>
//-自己写mutex类
class noncopyable{
    public:
    noncopyable()=default;
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};
class MutexLock:noncopyable{
private:
    //-线程锁
    pthread_mutex_t mutex_;
    //-持有锁的进程
    pid_t holder;
public:
    //-构造函数
    MutexLock():holder(0) {
        pthread_mutex_init(&mutex_, NULL);
    }
    //-是否被当前线程持有
    bool isLockedByThisThread(){
        
    }

};


#endif //ZYMMUDUO_DEMO1_H
