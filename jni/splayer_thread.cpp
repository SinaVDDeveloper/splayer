#include <errno.h>
#include <sys/time.h>
#include "splayer_thread.h"


u8 SPlayerMutext::create()
{
    mutex_ = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if(mutex_ == NULL)
    {
        return SPLAYER_MUTEX_ERROR_INIT;
    }
    s8 ret = pthread_mutex_init(mutex_, NULL);
    if(ret!=0)
    {
        return SPLAYER_MUTEX_ERROR_INIT;
    }
    return SPLAYER_MUTEX_ERROR_OK;
}

u8 SPlayerMutext::lock()
{
    s8 ret = pthread_mutex_lock(mutex_);
    if(ret != 0)
    {
        return SPLAYER_MUTEX_ERROR_LOCK;
    }
    return SPLAYER_MUTEX_ERROR_OK;
}

u8 SPlayerMutext::unLock()
{
    s8 ret = pthread_mutex_unlock(mutex_);
    if(ret!=0)
    {
        return SPLAYER_MUTEX_ERROR_UNLOCK;
    }
    return SPLAYER_MUTEX_ERROR_OK;
}

u8 SPlayerMutext::destroy()
{
    if(mutex_)
    {
        pthread_mutex_destroy(mutex_);
        free(mutex_);
    }
    return SPLAYER_MUTEX_ERROR_OK;
}


u8 SPlayerCondition::create()
{
    condition_ = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    if(condition_ == NULL)
    {
        return SPLAYER_CONDITION_ERROR_INIT;
    }
    int ret = pthread_cond_init(condition_, NULL);
    if(ret != 0)
    {
        return SPLAYER_CONDITION_ERROR_INIT;
    }
    return SPLAYER_CONDITION_ERROR_OK;
}

void SPlayerCondition::setMutex(pthread_mutex_t *mutex)
{
    mutex_ = mutex;
}

u8 SPlayerCondition::destroy()
{
    if(condition_)
    {
        pthread_cond_destroy(condition_);
        free(condition_);
    }
    return SPLAYER_CONDITION_ERROR_OK;
}

u8 SPlayerCondition::singal()
{
    return pthread_cond_signal(condition_);
}

u8 SPlayerCondition::wait()
{
    return pthread_cond_wait(condition_, mutex_);
}

u8 SPlayerCondition::timeWait(int ms)
{
    u8 ret = 0;
    struct timeval delta;
    struct timespec absTime;
    
    gettimeofday(&delta, NULL);
    absTime.tv_sec = delta.tv_sec + (ms/1000);
    absTime.tv_nsec = (delta.tv_usec + (ms % 1000) * 1000) * 1000;
    if ( absTime.tv_nsec > 1000000000 ) {
        absTime.tv_sec += 1;
        absTime.tv_nsec -= 1000000000;
    }
tryagain:
    ret = pthread_cond_timedwait(condition_, mutex_, &absTime);
    switch (ret) {
        case EINTR:
            goto tryagain;
            break;
        case ETIMEDOUT:
            ret = 1;
            break;
        case 0:
            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}


