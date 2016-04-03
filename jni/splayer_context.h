#ifndef __splayer__splayer_context__
#define __splayer__splayer_context__

#include <stdio.h>
#include <list>
#include "splayer_thread.h"
#include "audio_device_opensles.h"
using namespace std;

#define SPLAYER_CONTEXT_ERROR_OK 0;

class SPlayerEvent
{
public:
private:
    int type_;
    void *data_;
public:
    SPlayerEvent *getEvent();
    void setEvent(int type,void *data);
};

/// 上下文
class SPlayerContext
{
public:
private:
    list<splayer_event> queue_;
    SPlayerMutext mutex_;
    AudioDeviceAndroidOpenSLES *opensles_;
public:
    int push(SPlayerEvent event);
    int peep(SPlayerEvent event,int type);
    int wait(SPlayerEvent event);
};

#endif /* defined(__splayer__splayer_context__) */
