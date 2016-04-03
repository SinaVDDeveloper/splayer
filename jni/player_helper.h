#ifndef __LYH_PALYER_HELPER_H__
#define __LYH_PALYER_HELPER_H__

#include <list>
#include <pthread.h>
#include <jni.h>
#include "video_render_defines.h"
#include "player_jni_entry.h"

class AndroidNativeOpenGl2Renderer;
class AudioDeviceAndroidOpenSLES;
//class JNIAndroidOpenGl2RendererListener;

#ifdef __cplusplus
extern "C" {
#endif

//init
int LYH_Init(int arg);

//destroy
void LYH_Quit();

///mutex
#define LYH_Mutex pthread_mutex_t

LYH_Mutex* LYH_CreateMutex();

int LYH_LockMutex(LYH_Mutex* mtx);

int LYH_UnlockMutex(LYH_Mutex* mtx);

int LYH_DestroyMutex(LYH_Mutex* mtx);

///cond
#define LYH_Cond pthread_cond_t

LYH_Cond* LYH_CreateCond();

int LYH_SignalCond(LYH_Cond* cond);

int LYH_WaitCond(LYH_Cond* cond, LYH_Mutex* mtx);

int LYH_TimeWaitCond(LYH_Cond* cond, LYH_Mutex* mtx, int ms);

int LYH_DestroyCond(LYH_Cond* cond);

//event
#define LYH_ALLEVENTS 					0
#define LYH_ALLOC_EVENT   				1 
#define LYH_QUIT_EVENT    				2  
#define LYH_REFRESH_EVENT 				3
#define LYH_PAUSE_EVENT   				4
#define LYH_SEEK_EVENT    				5
#define LYH_RESET_EVENT    				6
#define LYH_SET_VOLUME_EVENT    		7



typedef struct{
	int type;
	void *data;
}LYH_event;

///context
typedef struct LYH_Context{
	std::list<LYH_event> lyh_event_queue;
	LYH_Mutex* lyh_event_queue_mtx;
	AudioDeviceAndroidOpenSLES* audio_device_android_opensles;
}LYH_Context;


int LYH_PushEvent(LYH_Context* context,LYH_event *event);

int LYH_PeepEvents(LYH_Context* context,LYH_event *event, int type);

int LYH_WaitEvent(LYH_Context* context,LYH_event *event);

///delay
int LYH_Delay(int ms);

///thread
#define LYH_Thread pthread_t

LYH_Thread LYH_CreateThread( void* (*run)(void *arg), void *data); //return thread id
int LYH_WaitThread(LYH_Thread id, void ** ret_val);

typedef struct { 
	int x;
	int y;
	int w;
	int h; 
}LYH_Rect;

///video
#define LYH_Surface AndroidNativeOpenGl2Renderer	
#define LYH_OpenGL2RenderListener JNIAndroidOpenGl2RendererListener

LYH_Surface* LYH_OpenSurface(int wantedWidth, int wantedHeight,LYH_OpenGL2RenderListener* opengl2_render_listener);	
int LYH_CloseSurface(LYH_Surface* sur);  
int LYH_SetVideoMode(int mode);

typedef struct LYH_Overlay {
	unsigned int format;				/**< Read-only */
	int w, h;				/**< Read-only */
	int planes;				/**< Read-only */
	int pitches[4];			/**< Read-only */
	unsigned char *pixels[4];				/**< Read-write */
	LYH_Surface* render;
	LYH_Mutex* mutex;
	VideoFrame* frame;
} LYH_Overlay;

int LYH_LockYUVOverlay(LYH_Overlay* overlay);
int LYH_UnlockYUVOverlay(LYH_Overlay* overlay);
LYH_Overlay* LYH_CreateYUVOverlay(int width, int height, int format, LYH_Surface* render);
int LYH_FreeYUVOverlay(LYH_Overlay* overlay);
int LYH_DisplayYUVOverlay(LYH_Overlay* overlay, LYH_Rect* rect);
///dispaly last frame
int LYH_ReDisplayYUVOverlay(LYH_Overlay* overlay);

int LYH_MapRGB(int a, int b, int c);
int LYH_FillRect(LYH_Surface* screen, LYH_Rect* rect, int color);
int LYH_UpdateRect(LYH_Surface* screen, int x, int y, int w, int h);

///aduio
#define LYH_AUDIO_S16 16
typedef struct LYH_AudioSpec{
	int channels; ///Number of channels: 1 mono, 2 stereo
    int freq;
	int format; ///Audio data format
    int silence; ///Audio buffer silence value (calculated)
    int samples; ///Audio buffer size in samples (power of 2)
    void (*callback)(void *opaque, char *stream, int len);
    void *userdata;
	int size; ///Audio buffer size in samples (in bytes)
}LYH_AudioSpec;
				
int LYH_OpenAudio(LYH_Context* context,LYH_AudioSpec* wanted_spec, LYH_AudioSpec* spec);	
int LYH_PauseAudio(LYH_Context* context,int arg); //play voice
int LYH_CloseAudio(LYH_Context* context);   



		
#ifdef __cplusplus
}
#endif

#endif
