#ifndef ANDROID_VIDEO_RENDER_OPENGLES20_H_
#define ANDROID_VIDEO_RENDER_OPENGLES20_H_

#include "video_render_defines.h"
#include "player_jni_entry.h"


#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


class VideoRenderOpenGles20 {
 public:
  VideoRenderOpenGles20(JNIAndroidOpenGl2RendererListener* opengl2_render_listener);
  ~VideoRenderOpenGles20();

  int Setup(int widht, int height);
  int Render(VideoFrame& frameToRender); ///int Render(const VideoFrame& frameToRender);
  int SetCoordinates(int zOrder,
                               const float left,
                               const float top,
                               const float right,
                               const float bottom);

 private:
  void printGLString(const char *name, GLenum s);
  void checkGlError(const char* op);
  GLuint loadShader(GLenum shaderType, const char* pSource);
  GLuint createProgram(const char* pVertexSource,
                       const char* pFragmentSource);
  void SetupTextures(VideoFrame& frameToRender); ///void SetupTextures(const VideoFrame& frameToRender);
  void UpdateTextures(VideoFrame& frameToRender); ///void UpdateTextures(const VideoFrame& frameToRender);

  int _id;
  GLuint _textureIds[3]; // Texture id of Y,U and V texture.
  GLuint _program;
  GLuint _vPositionHandle;
  GLsizei _textureWidth;
  GLsizei _textureHeight;
  int _width;
  int _height;

  GLfloat _vertices[20];
  static const char g_indices[];

  static const char g_vertextShader[];
  static const char g_fragmentShader[];

  int _sufacechanged;
  int _setup;  ///播放时有时候有声音没视频
  JNIAndroidOpenGl2RendererListener* _opengl2_render_listener;

};


#endif  // ANDROID_VIDEO_RENDER_OPENGLES20_H_
