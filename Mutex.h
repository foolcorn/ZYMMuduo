#ifndef ZYMMUDUO_MUTEX_H
#define ZYMMUDUO_MUTEX_H

#include<unistd.h>
#include<pthread.h>
#include <assert.h>

//-假的线程类
class CurrentThread{
public:
    static pid_t tid(){
        return 1;
    }
};
class noncopyable{
    public:
    noncopyable()=default;
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};
//-自己写mutex类
class MutexLock: public noncopyable{
private:
    //-线程锁，不该用底层的同步机制？
    pthread_mutex_t mutex_{};
    //-持有锁的进程/线程
    pid_t holder_;
public:
    //-构造函数
    MutexLock():holder_(0) {
        pthread_mutex_init(&mutex_, NULL);
    }
    ~MutexLock(){
        //-判断当前是否有线程持有锁,没有线程退出程序
        assert(holder_==0);
        pthread_mutex_destroy(&mutex_);
    }
    //-是否被当前线程持有
    bool isLockedByThisThread(){
        return holder_ == CurrentThread::tid();
    }
    //-调试是否被锁
    void assertLocked(){
        assert(isLockedByThisThread());
    }
    //-加锁,为什么没返回值？
    //!仅供mutexlockguard调用，用户不准调用
    void lock(){
        pthread_mutex_lock(&mutex_);
        holder_ = CurrentThread::tid();
    }
    //-解锁
    void unlock(){
        holder_ = 0;
        pthread_mutex_unlock(&mutex_);
    }
    //-获取当前的mutex_变量
    //!仅供condiion调用，用户不准调用
    pthread_mutex_t* getPthreadMutexn(){
        return &mutex_;
    }
};

//-lockguard类
class MutexLockGuard : public noncopyable{
private:
    MutexLock& mutex_;
public:
    explicit MutexLockGuard(MutexLock& mutex):mutex_(mutex){
        mutex_.lock();
    }
    //-lockguard类析构的时候自动销毁mutexlock类
    ~MutexLockGuard(){
        mutex_.unlock();
    }
};
//-这个宏是为了防止 MutexLockGuard(mutex)这种忘了加变量名情况，导致临时对象构建后立马销毁
# define MutexLockGuard(x) static_assert(false, "缺少lock guard的变量名")

//-condition类(条件变量)
class Condition : public noncopyable{
private:
    MutexLock& mutex_;
    pthread_cond_t pcond_;
public:
    explicit Condition(MutexLock& mutex) : mutex_(mutex){
        pthread_cond_init(&pcond_, NULL);
    }
    ~Condition(){
        pthread_cond_destroy(&pcond_);
    }
    void wait(){
        pthread_cond_wait(&pcond_, mutex_.getPthreadMutexn());
    }
    void notify(){
        pthread_cond_signal(&pcond_);
    }
    void notifyAll(){
        pthread_cond_broadcast(&pcond_);
    }
};



#endif //ZYMMUDUO_MUTEX_H
