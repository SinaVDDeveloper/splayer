#include <player_helper.h>
#include <list>
#include <unistd.h> 
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include "player_log.h"
#include "video_render_android.h"
#include "audio_device_opensles.h"

#ifdef __cplusplus
extern "C" {
#endif


LYH_Mutex* LYH_CreateMutex()
{
	LYH_Mutex* mtx = (LYH_Mutex*)malloc(sizeof(LYH_Mutex));
	if(mtx==NULL){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s malloc fail", __FUNCTION__);
		return NULL;
	}
	int res = pthread_mutex_init(mtx,NULL);
	if(0!=res){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s pthread_mutex_init fail", __FUNCTION__);
		return NULL;
	}
	
	return mtx;	   
}

int LYH_LockMutex(LYH_Mutex* mtx)
{
	return pthread_mutex_lock(mtx);
}

int LYH_UnlockMutex(LYH_Mutex* mtx)
{
	return pthread_mutex_unlock(mtx);
}

int LYH_DestroyMutex(LYH_Mutex* mtx)
{
	if(mtx){
		pthread_mutex_destroy(mtx);
		free(mtx);
		return 0;
	}
	return -1;
}

LYH_Cond* LYH_CreateCond()
{
	LYH_Cond* cond = (LYH_Cond*)malloc(sizeof(LYH_Cond));
	if(cond==NULL){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s malloc fail", __FUNCTION__);
		return NULL;
	}
	int res = pthread_cond_init(cond,NULL);
	if(0!=res){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s pthread_cond_init fail", __FUNCTION__);
		return NULL;
	}
	
	return cond;	
}

int LYH_SignalCond(LYH_Cond* cond)
{
	return pthread_cond_signal(cond);
}

int LYH_WaitCond(LYH_Cond* cond, LYH_Mutex* mtx)
{
	return pthread_cond_wait(cond, mtx);
}
/*
int LYH_TimeWaitCond(LYH_Cond* cond, LYH_Mutex* mtx, int ms)
{
	struct timespec to;
    to.tv_sec = time(NULL) + ms/1000;
    to.tv_nsec = (ms%1000)*1000000;
	return pthread_cond_timedwait(cond, mtx, &to);
}*/

int LYH_TimeWaitCond(LYH_Cond* cond, LYH_Mutex* mtx, int ms)
{

	int retval;
	struct timeval delta;
	struct timespec abstime;


	gettimeofday(&delta, NULL);

	abstime.tv_sec = delta.tv_sec + (ms/1000);
	abstime.tv_nsec = (delta.tv_usec + (ms%1000) * 1000) * 1000;
        if ( abstime.tv_nsec > 1000000000 ) {
          abstime.tv_sec += 1;
          abstime.tv_nsec -= 1000000000;
        }

  tryagain:
	retval = pthread_cond_timedwait(cond, mtx, &abstime);
	switch (retval) {
	case EINTR:
		goto tryagain;
		break;
	case ETIMEDOUT:
		retval = 1;
		break;
	case 0:
		break;
	default:
		retval = -1;
		break;
	}
	return retval;
}


int LYH_DestroyCond(LYH_Cond* cond)
{
	if(cond){
		pthread_cond_destroy(cond);
		free(cond);
		return 0;
	}
	return -1;
}


int LYH_PushEvent(LYH_Context* context,LYH_event *event)
{
	if(!context){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s context NULL", __FUNCTION__);
		return -1;
	}
	if(!event){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s event NULL", __FUNCTION__);
		return -1;
	}
	LYH_event e;
	e.type = event->type;
	e.data = event->data;
	
	if(NULL==context->lyh_event_queue_mtx){
		context->lyh_event_queue_mtx = LYH_CreateMutex();
		if(NULL==context->lyh_event_queue_mtx){
			//STrace::Log(SPLAYER_TRACE_ERROR,"%s LYH_CreateMutex fail", __FUNCTION__);
			return -1;
		}
	}
	
	LYH_LockMutex(context->lyh_event_queue_mtx);
	
	context->lyh_event_queue.push_back( e );
	
	LYH_UnlockMutex(context->lyh_event_queue_mtx);
	
	return 0;
}

//return event count 1 if have, 0 none event , -1 error
int LYH_PeepEvents(LYH_Context* context,LYH_event *event, int type)
{
	if(!context){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s context NULL", __FUNCTION__);
		return -1;
	}

	if(!event){
		//STrace::Log(SPLAYER_TRACE_ERROR,"%s event NULL", __FUNCTION__);
		return -1;
	}
	if(NULL==context->lyh_event_queue_mtx){
		context->lyh_event_queue_mtx = LYH_CreateMutex();
		if(NULL==context->lyh_event_queue_mtx){
			//STrace::Log(SPLAYER_TRACE_ERROR,"%s LYH_CreateMutex fail", __FUNCTION__);
			return -1;
		}
	}
	
	LYH_LockMutex(context->lyh_event_queue_mtx);
	
	if(0==type){
		if(!context->lyh_event_queue.empty()){
			event->type  = context->lyh_event_queue.front().type;
			event->data  =  context->lyh_event_queue.front().data;
			context->lyh_event_queue.pop_front( );
			
			LYH_UnlockMutex(context->lyh_event_queue_mtx);
			return 1;
		}
	}else{
		std::list<LYH_event>::iterator it;  
		for (it=context->lyh_event_queue.begin(); it!=context->lyh_event_queue.end(); it++){
			if(type==(*it).type){
				event->type  =  (*it).type;
				event->data  =  (*it).data;
				context->lyh_event_queue.erase( it );
				
				LYH_UnlockMutex(context->lyh_event_queue_mtx);
				return 1;
			}
		}
	}
	LYH_UnlockMutex(context->lyh_event_queue_mtx);
	
	return 0;
}

int LYH_WaitEvent(LYH_Context* context,LYH_event *event)
{
    while ( 1 ) {
        switch(LYH_PeepEvents(context,event, LYH_ALLEVENTS)) {
            case -1: return 0;
            case 1: return 1;
            case 0: LYH_Delay(10);
        }
    }
}
/*
int LYH_Delay(int ms)
{
	pthread_cond_t cond;
	pthread_mutex_t mutex;

    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&cond,NULL);

    struct timespec to;
    to.tv_sec = time(NULL) + ms/1000;
    to.tv_nsec = (ms%1000)*1000000;

    pthread_mutex_lock(&mutex);
	pthread_cond_timedwait(&cond, &mutex, &to);
    pthread_mutex_unlock(&mutex);

	return 0;
}*/


int LYH_Delay(int ms)
{
	int retval;
	struct timeval delta;
	struct timespec abstime;
	pthread_cond_t cond;
	pthread_mutex_t mutex;

    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&cond,NULL);


	gettimeofday(&delta, NULL);

	abstime.tv_sec = delta.tv_sec + (ms/1000);
	abstime.tv_nsec = (delta.tv_usec + (ms%1000) * 1000) * 1000;
        if ( abstime.tv_nsec > 1000000000 ) {
          abstime.tv_sec += 1;
          abstime.tv_nsec -= 1000000000;
        }

  tryagain:
	pthread_mutex_lock(&mutex);
	retval = pthread_cond_timedwait(&cond, &mutex, &abstime);
	pthread_mutex_unlock(&mutex);
	switch (retval) {
	    case EINTR:
		goto tryagain;
		break;
	    case ETIMEDOUT:
		retval = 1;
		break;
	    case 0:
		break;
	    default:
		retval = -1;
		break;
	}
	return retval;
}

/*
LYH_Thread LYH_CreateThread( void* (*run)(void *arg), void *data)
{
	//STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
	int err;
	LYH_Thread id;
	err = pthread_create(&id,NULL,run,data);
	if(err != 0){
		return -1;
	}
	return id;
}
*/

//thread id should not equal 0
LYH_Thread LYH_CreateThread( void* (*run)(void *arg), void *data)
{
	int err;
	LYH_Thread id = 0;
	err = pthread_create(&id,NULL,run,data);
	if(err != 0){
		return 0;
	}
	return id;
}


int LYH_WaitThread(LYH_Thread id, void ** ret_val)
{
	return pthread_join(id, ret_val);
}


LYH_Overlay* LYH_CreateYUVOverlay(int width, int height, int format, LYH_Surface* render)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into,width=%d height=%d format=%d", __FUNCTION__,width,height,format);
	LYH_Overlay* overlay = (LYH_Overlay*)malloc(sizeof(LYH_Overlay));
	if(NULL==overlay){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s malloc fail", __FUNCTION__);
		return NULL;
	}
	overlay->format = format;				/**< Read-only */
	overlay->w = width;
	overlay->h = height;				/**< Read-only */
	overlay->planes = 3;				/**< Read-only */
	
	//unsigned char *buf = (unsigned char *)malloc(width*height*2);	
	int length = width*height*2; //(width*height*3)/2;
	///STrace::Log("%s step 1.1", __FUNCTION__);
	overlay->frame = new  VideoFrame();
	if(!overlay->frame){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s new  VideoFrame fail", __FUNCTION__);
		return NULL;
	}
	overlay->frame->VerifyAndAllocate( length );
	///STrace::Log("%s step 1.2", __FUNCTION__);
	length = (width*height*3)/2;
	overlay->frame->SetLength( length );
	overlay->frame->SetWidth(width);
	overlay->frame->SetHeight(height);
	///STrace::Log("%s step 2", __FUNCTION__);
	overlay->pixels[0] =	overlay->frame->Buffer();		/**< Read-write */
	overlay->pixels[1] =	overlay->pixels[0] + width*height;		/**< Read-write */
	overlay->pixels[2] =	overlay->pixels[1] + (width*height)/4;		/**< Read-write */
	overlay->pixels[3] =	overlay->pixels[2] + (width*height)/4;		/**< Read-write */
	overlay->pitches[0] = width;			/**< Read-only */
	overlay->pitches[1] = width/2;			/**< Read-only */
	overlay->pitches[2] = width/2;			/**< Read-only */
	overlay->pitches[3] = width/2;			/**< Read-only */
	
	overlay->render = render;
	///STrace::Log("%s step 3", __FUNCTION__);
	overlay->mutex =  LYH_CreateMutex();
	if(NULL==overlay->mutex){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s LYH_CreateMutex fail", __FUNCTION__);
		return NULL;
	}
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
	return overlay;
}

int LYH_FreeYUVOverlay(LYH_Overlay* overlay)
{
	//unsigned char *buf = overlay->pixels[0];
	//free(buf);
	if(overlay->frame){
		overlay->frame->Free();
		delete overlay->frame;
	}
	
	LYH_DestroyMutex(overlay->mutex);
	free(overlay);
}

int LYH_LockYUVOverlay(LYH_Overlay* overlay)
{
	return LYH_LockMutex(overlay->mutex);
}

int LYH_UnlockYUVOverlay(LYH_Overlay* overlay)
{
	return LYH_UnlockMutex(overlay->mutex);
}


int LYH_DisplayYUVOverlay(LYH_Overlay* overlay, LYH_Rect* rect)
{
	if(! overlay->render){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s render NULL", __FUNCTION__);
		return -1;
	}
	return overlay->render->RenderFrame(overlay->frame);
}


int LYH_ReDisplayYUVOverlay(LYH_Overlay* overlay)
{
	if(! overlay->render){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s render NULL", __FUNCTION__);
		return -1;
	}
	return overlay->render->ReRender();

}


int LYH_MapRGB(int a, int b, int c)
{
	return 0;
}
int LYH_FillRect(LYH_Surface* screen, LYH_Rect* rect, int color)
{
	return 0;
}
int LYH_UpdateRect(LYH_Surface* screen, int x, int y, int w, int h)
{
	return 0;
}


///android audio device
//static AudioDeviceAndroidOpenSLES* androidOutputDevice = NULL;
int LYH_OpenAudio(LYH_Context* context,LYH_AudioSpec* wanted_spec, LYH_AudioSpec* spec)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into(wanted_spec->channels=%d,wanted_spec->freq=%d,wanted_spec->format=%d,wanted_spec->samples=%d)", 
		__FUNCTION__, wanted_spec->channels, wanted_spec->freq,wanted_spec->format,wanted_spec->samples);
	int res;
	
	spec->channels = 1; ///Number of channels: 1 mono, 2 stereo
    spec->freq = 16000;
	spec->format = LYH_AUDIO_S16; ///Audio data format
    spec->silence = 0; ///Audio buffer silence value (calculated)
    spec->samples = 160*3; ///3 of 10ms 16000HZ sample rate
    spec->callback = wanted_spec->callback;
    spec->userdata = wanted_spec->userdata;
	spec->size = 2*spec->channels*spec->samples; ///Audio buffer size in samples (in bytes)
	
	
	if(NULL==context->audio_device_android_opensles)
		context->audio_device_android_opensles = new  AudioDeviceAndroidOpenSLES(0);
	
	context->audio_device_android_opensles->AttachAudioBuffer(spec->callback,spec->userdata);
	
	res = context->audio_device_android_opensles->Init();
	if(0!=res){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s init android device fail",__FUNCTION__);
		return -1;
	}
	//bool ok;
	res = context->audio_device_android_opensles->InitPlayout();
	if(0!=res){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s android device not available",__FUNCTION__);
		return -1;
	}

	//if(ok==true){
	//	//set data offer callback
	//}else{
	//	STrace::Log("%s android device not available",__FUNCTION__);
	//	return -1;
	//}

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
	return 0;
}

int LYH_PauseAudio(LYH_Context* context,int start)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into,start=%d", __FUNCTION__,start);
	if(!context->audio_device_android_opensles){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s androidOutputDevice NULL", __FUNCTION__);
		return -1;
	}
	
	if(start){
		return context->audio_device_android_opensles->StartPlayout();
	}else{
		return context->audio_device_android_opensles->StopPlayout();
	}
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
	return 0;
}

int LYH_CloseAudio(LYH_Context* context)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
	if(context->audio_device_android_opensles){
		context->audio_device_android_opensles->StopPlayout();
		context->audio_device_android_opensles->Terminate();
		delete context->audio_device_android_opensles;
		context->audio_device_android_opensles = NULL;
	}
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
}

//static AndroidNativeOpenGl2Renderer androidRenderDevice = NULL;
LYH_Surface* LYH_OpenSurface(int wantedWidth, int wantedHeight,JNIAndroidOpenGl2RendererListener* opengl2_render_listener)
{	
	STrace::Log(SPLAYER_TRACE_DEBUG,"LYH_OpenSurface into,wantedWidth=%d,wantedHeight=%d",wantedWidth,wantedHeight);
	AndroidNativeOpenGl2Renderer *render = new AndroidNativeOpenGl2Renderer(opengl2_render_listener);
	if(!render){
		STrace::Log(SPLAYER_TRACE_ERROR,"AndroidNativeOpenGl2Renderer new fail");
		return NULL;
	}
	int res = render->Init();
	if(0!=res){
		STrace::Log(SPLAYER_TRACE_ERROR,"AndroidNativeOpenGl2Renderer Init() fail");
		return NULL;
	}
	return render;
}

int LYH_CloseSurface(LYH_Surface* sur)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"LYH_CloseSurface into");
	if(sur){
		delete sur;
	}
}

int LYH_SetVideoMode(int mode)
{
	return 0;
}

int LYH_Init(int arg)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);

	return 0;
}

void LYH_Quit()
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
}

#ifdef __cplusplus
}
#endif
