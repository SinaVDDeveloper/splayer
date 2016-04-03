#ifndef __splayer__player_thread__
#define __splayer__player_thread__

#include <stdio.h>
#include <pthread.h>
#include "global.h"

#define SPLAYER_MUTEX_ERROR_OK 0;
#define SPLAYER_MUTEX_ERROR_INIT 1;
#define SPLAYER_MUTEX_ERROR_LOCK 2;
#define SPLAYER_MUTEX_ERROR_UNLOCK 2;

/// 锁
class SPlayerMutext
{
public:
    
private:
    pthread_mutex_t *mutex_;
public:
    u8 create();
    u8 destroy();
    u8 lock();
    u8 unLock();
};

#define SPLAYER_CONDITION_ERROR_OK 0;
#define SPLAYER_CONDITION_ERROR_INIT 1;
#define SPLAYER_CONDITION_ERROR_OK 0;
#define SPLAYER_CONDITION_ERROR_OK 0;
#define SPLAYER_CONDITION_ERROR_OK 0;

/// 条件
class SPlayerCondition
{
public:
    
private:
    pthread_cond_t *condition_;
    pthread_mutex_t *mutex_;
public:
    u8 create();
    void setMutex(pthread_mutex_t *mutex);
    u8 destroy();
    u8 wait();
    u8 timeWait(int ms);
    u8 singal();
};

#endif /* defined(__splayer__player_thread__) */
