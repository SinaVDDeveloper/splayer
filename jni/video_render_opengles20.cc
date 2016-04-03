#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>

#include "video_render_opengles20.h"
#include "player_log.h"

#define OPENGLES_RENDER_DEBUG 1

#define OPENGLES_ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))

const char VideoRenderOpenGles20::g_indices[] = { 0, 3, 2, 0, 2, 1 };

const char VideoRenderOpenGles20::g_vertextShader[] = {
  "attribute vec4 aPosition;\n"
  "attribute vec2 aTextureCoord;\n"
  "varying vec2 vTextureCoord;\n"
  "void main() {\n"
  "  gl_Position = aPosition;\n"
  "  vTextureCoord = aTextureCoord;\n"
  "}\n" };

// The fragment shader.
// Do YUV to RGB565 conversion.
//texture2D(texture,textureCoordinate) 读取在当前纹理坐标中纹理的值
const char VideoRenderOpenGles20::g_fragmentShader[] = {
  "precision mediump float;\n"
  "uniform sampler2D Ytex;\n"
  "uniform sampler2D Utex,Vtex;\n"
  "varying vec2 vTextureCoord;\n"
  "void main(void) {\n"
  "  float nx,ny,r,g,b,y,u,v;\n"
  "  mediump vec4 txl,ux,vx;"
  "  nx=vTextureCoord[0];\n"
  "  ny=vTextureCoord[1];\n"
  "  y=texture2D(Ytex,vec2(nx,ny)).r;\n"
  "  u=texture2D(Utex,vec2(nx,ny)).r;\n"
  "  v=texture2D(Vtex,vec2(nx,ny)).r;\n"

  //"  y = v;\n"+
  "  y=1.1643*(y-0.0625);\n"
  "  u=u-0.5;\n"
  "  v=v-0.5;\n"

  "  r=y+1.5958*v;\n"
  "  g=y-0.39173*u-0.81290*v;\n"
  "  b=y+2.017*u;\n"
  "  gl_FragColor=vec4(r,g,b,1.0);\n"
  "}\n" };



VideoRenderOpenGles20::VideoRenderOpenGles20(JNIAndroidOpenGl2RendererListener* opengl2_render_listener) :
    _opengl2_render_listener(opengl2_render_listener),
    _textureWidth(-1),
    _textureHeight(-1),
    _width(-1),
    _height(-1),
    _sufacechanged(0),
    _setup(0){
  	STrace::Log( SPLAYER_TRACE_DEBUG,"%s: id %d",__FUNCTION__, (int) _id);

	/// X Y Z -> g_vertextShader aPosition; U V -> g_vertextShader aTextureCoord
	const GLfloat vertices[20] = {
    // X, Y, Z, U, V
    -1, -1, 0, 0, 1, // Bottom Left
    1, -1, 0, 1, 1, //Bottom Right
    1, 1, 0, 1, 0, //Top Right
    -1, 1, 0, 0, 0 }; //Top Left
  
  	memcpy(_vertices, vertices, sizeof(_vertices));
}

VideoRenderOpenGles20::~VideoRenderOpenGles20() {
}

int VideoRenderOpenGles20::Setup(int width,int height) {
  
  STrace::Log(SPLAYER_TRACE_DEBUG,"%s: width %d, height %d", __FUNCTION__, (int) width,(int) height);

  printGLString("Version", GL_VERSION);
  printGLString("Vendor", GL_VENDOR);
  printGLString("Renderer", GL_RENDERER);
  printGLString("Extensions", GL_EXTENSIONS);

  int maxTextureImageUnits[2];
  int maxTextureSize[2];
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, maxTextureImageUnits);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, maxTextureSize);

  STrace::Log(SPLAYER_TRACE_DEBUG,"%s: number of textures %d, size %d", __FUNCTION__,
               (int) maxTextureImageUnits[0], (int) maxTextureSize[0]);

  _program = createProgram(g_vertextShader, g_fragmentShader);
  if (!_program) {
    STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not create program", __FUNCTION__);
    return -1;
  }

  int positionHandle = glGetAttribLocation(_program, "aPosition");
  checkGlError("glGetAttribLocation aPosition");
  if (positionHandle == -1) {
    STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not get aPosition handle", __FUNCTION__);
    return -1;
  }

  int textureHandle = glGetAttribLocation(_program, "aTextureCoord");
  checkGlError("glGetAttribLocation aTextureCoord");
  if (textureHandle == -1) {
    STrace::Log(SPLAYER_TRACE_ERROR, "%s: Could not get aTextureCoord handle", __FUNCTION__);
    return -1;
  }

  // set the vertices array in the shader
  // _vertices contains 4 vertices with 5 coordinates.
  // 3 for (xyz) for the vertices and 2 for the texture
  glVertexAttribPointer(positionHandle, 3, GL_FLOAT, false,
                        5 * sizeof(GLfloat), _vertices);
  checkGlError("glVertexAttribPointer aPosition");

  glEnableVertexAttribArray(positionHandle);
  checkGlError("glEnableVertexAttribArray positionHandle");

  // set the texture coordinate array in the shader
  // _vertices contains 4 vertices with 5 coordinates.
  // 3 for (xyz) for the vertices and 2 for the texture
  glVertexAttribPointer(textureHandle, 2, GL_FLOAT, false, 5
                        * sizeof(GLfloat), &_vertices[3]);
  checkGlError("glVertexAttribPointer maTextureHandle");
  glEnableVertexAttribArray(textureHandle);
  checkGlError("glEnableVertexAttribArray textureHandle");

  glUseProgram(_program);
  int i = glGetUniformLocation(_program, "Ytex");
  checkGlError("glGetUniformLocation");
  glUniform1i(i, 0); /* Bind Ytex to texture unit 0 */
  checkGlError("glUniform1i Ytex");

  i = glGetUniformLocation(_program, "Utex");
  checkGlError("glGetUniformLocation Utex");
  glUniform1i(i, 1); /* Bind Utex to texture unit 1 */
  checkGlError("glUniform1i Utex");

  i = glGetUniformLocation(_program, "Vtex");
  checkGlError("glGetUniformLocation");
  glUniform1i(i, 2); /* Bind Vtex to texture unit 2 */
  checkGlError("glUniform1i");

  //glViewport(0, 0, width, height); //glViewport(0, 0, width, height);
  //checkGlError("glViewport");
  
  _width = width;
  _height = height;
   STrace::Log(SPLAYER_TRACE_DEBUG,"%s: _width=%d,_height=%d", __FUNCTION__,_width,_height);
  _sufacechanged = 1;
  _setup = 1;
  return 0;
}

// SetCoordinates
// Sets the coordinates where the stream shall be rendered.
// Values must be between 0 and 1.
int VideoRenderOpenGles20::SetCoordinates(int zOrder,
                                                    const float left,
                                                    const float top,
                                                    const float right,
                                                    const float bottom) {
  if ((top > 1 || top < 0) || (right > 1 || right < 0) ||
      (bottom > 1 || bottom < 0) || (left > 1 || left < 0)) {
    STrace::Log(SPLAYER_TRACE_ERROR,"%s: Wrong coordinates", __FUNCTION__);
    return -1;
  }
  
  // Bottom Left
  _vertices[0] = (left * 2) - 1;
  _vertices[1] = -1 * (2 * bottom) + 1;
  _vertices[2] = zOrder;

  //Bottom Right
  _vertices[5] = (right * 2) - 1;
  _vertices[6] = -1 * (2 * bottom) + 1;
  _vertices[7] = zOrder;

  //Top Right
  _vertices[10] = (right * 2) - 1;
  _vertices[11] = -1 * (2 * top) + 1;
  _vertices[12] = zOrder;

  //Top Left
  _vertices[15] = (left * 2) - 1;
  _vertices[16] = -1 * (2 * top) + 1;
  _vertices[17] = zOrder;

  return 0;
}

int VideoRenderOpenGles20::Render(VideoFrame& frameToRender) { //int VideoRenderOpenGles20::Render(const VideoFrame& frameToRender) {

  if (frameToRender.Length() == 0) {
    return -1;
  }

  if(!_setup){
  	 STrace::Log(SPLAYER_TRACE_WARNING, "%s: not setup yet\n", __FUNCTION__);
	 if(NULL!=_opengl2_render_listener){
	 	int rw_ , rh_;
	 	_opengl2_render_listener->getSize(rw_,rh_);
	 	Setup(rw_,rh_);
	 }else{
	  	STrace::Log(SPLAYER_TRACE_WARNING, "%s: _opengl2_render_listener NULL\n", __FUNCTION__);
		Setup(frameToRender.Width(),frameToRender.Height());
	 }
  }

  glUseProgram(_program);
  checkGlError("glUseProgram");

  if (_textureWidth != (GLsizei) frameToRender.Width() ||
      _textureHeight != (GLsizei) frameToRender.Height() || 
      _sufacechanged) {
    SetupTextures(frameToRender);
	if(_sufacechanged)
		_sufacechanged = 0;
  }
  else {
    UpdateTextures(frameToRender);
  }

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, g_indices); ///glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);  not OK, just test
  checkGlError("glDrawArrays");

  return 0;
}

//private method
GLuint VideoRenderOpenGles20::loadShader(GLenum shaderType,
                                         const char* pSource) {
  GLuint shader = glCreateShader(shaderType);
  if (shader) {
    glShaderSource(shader, 1, &pSource, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
      GLint infoLen = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
      if (infoLen) {
        char* buf = (char*) malloc(infoLen);
        if (buf) {
          glGetShaderInfoLog(shader, infoLen, NULL, buf);
          STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not compile shader %d: %s",
                       __FUNCTION__, shaderType, buf);
          free(buf);
        }
        glDeleteShader(shader);
        shader = 0;
      }
    }
  }
  return shader;
}

GLuint VideoRenderOpenGles20::createProgram(const char* pVertexSource,
                                            const char* pFragmentSource) {
  GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
  if (!vertexShader) {
    return 0;
  }

  GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
  if (!pixelShader) {
    return 0;
  }

  GLuint program = glCreateProgram();
  if (program) {
    glAttachShader(program, vertexShader);
    checkGlError("glAttachShader");
    glAttachShader(program, pixelShader);
    checkGlError("glAttachShader");
    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
      GLint bufLength = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
      if (bufLength) {
        char* buf = (char*) malloc(bufLength);
        if (buf) {
          glGetProgramInfoLog(program, bufLength, NULL, buf);
          STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not link program: %s",
                       __FUNCTION__, buf);
          free(buf);
        }
      }
      glDeleteProgram(program);
      program = 0;
    }
  }
  return program;
}

void VideoRenderOpenGles20::printGLString(const char *name, GLenum s) {
  const char *v = (const char *) glGetString(s);
  STrace::Log(SPLAYER_TRACE_DEBUG, "GL %s = %s\n", name, v);
}

void VideoRenderOpenGles20::checkGlError(const char* op) {
#ifdef OPENGLES_RENDER_DEBUG
  for (GLint error = glGetError(); error; error = glGetError()) {
    STrace::Log(SPLAYER_TRACE_WARNING,"after %s() glError (0x%x)\n", op, error);
  }
#else
  return;
#endif
}

void VideoRenderOpenGles20::SetupTextures(VideoFrame& frameToRender) { ///void VideoRenderOpenGles20::SetupTextures(const VideoFrame& frameToRender) {
  	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: width %d, height %d length %u", __FUNCTION__,
               frameToRender.Width(), frameToRender.Height(),
               frameToRender.Length());

  	const GLsizei width = frameToRender.Width();
  	const GLsizei height = frameToRender.Height();

    ///reset glviewport
	glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    float ww_ = _width;
	float wt_ = frameToRender.Width();
    float zoom_ = ww_/wt_;
	int h_ = zoom_*frameToRender.Height();
	if(h_>_height){
		float hw_ = _height;
		float ht_ = frameToRender.Height();
		zoom_ = hw_/ht_;
		int w_ = zoom_*frameToRender.Width();
		int x = (_width - w_)/2;
		if(x<0 || w_<0 || _height<0 ){  ///should check param
			STrace::Log(SPLAYER_TRACE_WARNING,"%s: param error, x=%d,w_=%d,_height=%d", __FUNCTION__,x,w_,_height);
			glViewport( 0, 0, width, height);
		}else{
			glViewport( x, 0, w_, _height);
		}
		//checkGlError("glViewport");
		for (GLint error = glGetError(); error; error = glGetError()) {
    		STrace::Log(SPLAYER_TRACE_WARNING,"%s: glViewport Fail, glError (0x%x),glViewport param(x=%d,y=0,width=%d,height=%d),_width=%d,_height=%d,width=%d,height=%d\n",__FUNCTION__, error,x,w_,_height,_width,_height,width,height);
  		}
	}else{
		int y = (_height - h_)/2;
		if(y<0 || _width<0 || h_<0 ){  ///should check param
			STrace::Log(SPLAYER_TRACE_WARNING,"%s: param error, y=%d,_width=%d,h_=%d", __FUNCTION__,y,_width,h_);
			glViewport( 0, 0, width, height);
		}else{
			glViewport(0, y, _width, h_);
		}
		//checkGlError("glViewport");
		for (GLint error = glGetError(); error; error = glGetError()) {
    		STrace::Log(SPLAYER_TRACE_WARNING,"%s: glViewport Fail, glError (0x%x),glViewport param(x=0,y=%d,width=%d,height=%d),_width=%d,_height=%d,width=%d,height=%d\n",__FUNCTION__, error,y,_width,h_,_width,_height,width,height);
  		}
	}

	///align bytes
  	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	///Generate	the Y, U and V texture
  	glGenTextures(3, _textureIds); 

  	///draw Y
	GLuint currentTextureId = _textureIds[0]; // Y
	glActiveTexture( GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, currentTextureId); //绑定纹理
	//设置过滤器
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//指定纹理
   	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE,
               (const GLvoid*) frameToRender.Buffer());

	///draw U
  	currentTextureId = _textureIds[1];
  	glActiveTexture( GL_TEXTURE1);
  	glBindTexture(GL_TEXTURE_2D, currentTextureId);

  	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  	const unsigned char* uComponent = frameToRender.Buffer() + width * height;
  	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,width/2, height/2, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, (const GLvoid*) uComponent);

	///draw V
  	currentTextureId = _textureIds[2];
  	glActiveTexture( GL_TEXTURE2);
  	glBindTexture(GL_TEXTURE_2D, currentTextureId);

  	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  	const unsigned char* vComponent = uComponent + (width * height) / 4;
  	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width/2, height/2, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, (const GLvoid*) vComponent);
  
  	checkGlError("SetupTextures");

  	_textureWidth = width;
  	_textureHeight = height;
}

void VideoRenderOpenGles20::UpdateTextures(VideoFrame& frameToRender) { ///void VideoRenderOpenGles20::UpdateTextures(const VideoFrame& frameToRender) {
	const GLsizei width = frameToRender.Width();
  	const GLsizei height = frameToRender.Height();

	 ///repeat at some mobile
     glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
  	GLuint currentTextureId = _textureIds[0]; // Y
  	glActiveTexture( GL_TEXTURE0);
  	glBindTexture(GL_TEXTURE_2D, currentTextureId);
  	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE,
                  GL_UNSIGNED_BYTE, (const GLvoid*) frameToRender.Buffer());

  	currentTextureId = _textureIds[1]; // U
  	glActiveTexture( GL_TEXTURE1);
  	glBindTexture(GL_TEXTURE_2D, currentTextureId);
  	const unsigned char* uComponent = frameToRender.Buffer() + width * height;
  	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2,
                  GL_LUMINANCE, GL_UNSIGNED_BYTE, (const GLvoid*) uComponent);

  	currentTextureId = _textureIds[2]; // V
  	glActiveTexture( GL_TEXTURE2);
  	glBindTexture(GL_TEXTURE_2D, currentTextureId);
  	const unsigned char* vComponent = uComponent + (width * height) / 4;
  	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2,
                  GL_LUMINANCE, GL_UNSIGNED_BYTE, (const GLvoid*) vComponent);
  
  	checkGlError("UpdateTextures");

}
