#ifndef  PLAY_JNI_ENTRY_H
#define  PLAY_JNI_ENTRY_H

#include <jni.h>

// ref-counted object for callbacks
class JNIMediaPlayerListener
{
public:
    JNIMediaPlayerListener(JavaVM* jvm_, jobject thiz, jobject weak_thiz);
    ~JNIMediaPlayerListener();
    void notify(int msg, int ext1, int ext2, void *obj );
private:
    JNIMediaPlayerListener();
    jclass      mClass;     // Reference to MediaPlayer class
    jobject     mObject;    // Weak ref to MediaPlayer Java object to call on
    JavaVM* 	jvm;
};

class JNIAndroidOpenGl2RendererListener
{
public:
	JNIAndroidOpenGl2RendererListener(JavaVM* jvm_,jobject weak_thiz);
	~JNIAndroidOpenGl2RendererListener();
	void registerObject(long object);
	void unRegisterObject(long context);
	void redraw();
	int getSize(int &width, int &height);
private:
    JNIAndroidOpenGl2RendererListener();
    jclass      mClass;
    jobject     mObject;
	JavaVM* 	jvm;
	jmethodID      _redrawCid;
	jmethodID      _registerNativeCID;
	jmethodID      _deRegisterNativeCID;
	jmethodID      _getWidthCid;
	jmethodID      _getHeightCid;
};


class JNIAndroidSurfaceRendererListener
{
public:
	JNIAndroidSurfaceRendererListener(JavaVM* jvm_,jobject weak_thiz);
	~JNIAndroidSurfaceRendererListener();
	jobject getSurface();
	jclass getMediaCodecClass();
private:
    JNIAndroidSurfaceRendererListener();
    jobject     mObject;
	jclass		mMediacodecClass;
	JavaVM* 	jvm;
};


#ifdef __cplusplus
extern "C" {
#endif



//extern function


#ifdef __cplusplus
}
#endif

#endif
