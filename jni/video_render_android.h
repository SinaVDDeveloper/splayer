#ifndef ANDROID_VIDEO_RENDER_ANDROID_NATIVE_OPENGL2_H_
#define ANDROID_VIDEO_RENDER_ANDROID_NATIVE_OPENGL2_H_

#include <jni.h>

#include "video_render_defines.h"
#include "video_render_opengles20.h"
#include "player_helper.h"

//class JNIAndroidOpenGl2RendererListener;

class AndroidNativeOpenGl2Channel{
public:
	AndroidNativeOpenGl2Channel(
		//int streamId,
		JNIAndroidOpenGl2RendererListener* opengl2_render_listener
		//JavaVM* jvm,
		//VideoRenderAndroid& renderer,
		//jobject javaRenderObj
		);
	~AndroidNativeOpenGl2Channel();

	int Init(	int zOrder,
                const float left,
                const float top,
                const float right,
                const float bottom );

	int RenderFrame(
      		const int streamId,
         	VideoFrame* videoFrame); ///VideoFrame& videoFrame);

	void DeliverFrame();

	int ReRender();
	
	void GetWidthHeight(int& width, int& height) { 
		if(_width>0){
			width = _width; 
			height = _height;
		}
	}
	static jint CreateOpenGLNativeStatic(
			  JNIEnv * env,
			  jobject,
			  jlong context,
			  jint width,
			  jint height);

	static void DrawNativeStatic(JNIEnv * env,jobject, jlong context);

private:
	
	jint CreateOpenGLNative(int width, int height);

	
	void DrawNative();
	
	int _id;
	//CriticalSectionWrapper& _renderCritSect;
	LYH_Mutex* _renderCritSect;

	VideoFrame _bufferToRender;
	//VideoRenderAndroid& _renderer;
	//JavaVM*     _jvm;
	//jobject     _javaRenderObj;
	//jmethodID      _redrawCid;
	//jmethodID      _registerNativeCID;
	//jmethodID      _deRegisterNativeCID;
	JNIAndroidOpenGl2RendererListener* _opengl2_render_listener; //release outside
	VideoRenderOpenGles20 _openGLRenderer;
	int _width, _height;
};


class AndroidNativeOpenGl2Renderer{
public:
	AndroidNativeOpenGl2Renderer(
									//JavaVM* jvm,
									//const int id,
								   //const VideoRenderType videoRenderType,
								   //void* window
								   //const bool fullscreen
								   JNIAndroidOpenGl2RendererListener* opengl2_render_listener
								   );
	~AndroidNativeOpenGl2Renderer();
	//bool UseOpenGL2(void* window);
	int Init();
	void GetWidthHeight(int& width, int& height);
	///new add
	int RenderFrame(VideoFrame* videoFrame); ///int RenderFrame(VideoFrame& videoFrame);

	int ReRender();
	
private:
	//virtual AndroidStream* CreateAndroidRenderChannel(
	AndroidNativeOpenGl2Channel* CreateAndroidRenderChannel(
		  int streamId,
		  int zOrder,
		  const float left,
		  const float top,
		  const float right,
		  const float bottom
		  //VideoRenderAndroid& renderer
		  );
	//JavaVM* g_jvm;	  
	//jobject _javaRenderObj;
	//jclass _javaRenderClass;
	///new add
	int _width, _height;
	//jobject _ptrWindow;
	JNIAndroidOpenGl2RendererListener* _opengl2_render_listener; //release outside
	AndroidNativeOpenGl2Channel* _openGLChannel;
	///
};

/*render usage 
// >2.3 use opengl render
remoteSurfaceView = ViERenderer.CreateRenderer(this,true);       
remoteVideo.render = remoteSurfaceView;
ret = session.CreateRemoteVideo(remoteVideo);
		///set remote render
		error = _vieRender->AddRenderer(_channel_id,_render, 0, 0.0f, 0.0f,1.0f,1.0f);
		error =  _vieRender->StartRender(_channel_id);
remoteSurfaceHolder.addView(remoteSurfaceView);
*/

#endif // ANDROID_VIDEO_RENDER_ANDROID_NATIVE_OPENGL2_H_
