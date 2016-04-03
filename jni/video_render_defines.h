#ifndef VIDEO_RENDER_DEFINES_H_
#define VIDEO_RENDER_DEFINES_H_

#include <stdlib.h>

/*************************************************
 *
 * VideoFrame class
 *
 * The VideoFrame class allows storing and
 * handling of video frames.
 *
 *
 *************************************************/
class VideoFrame
{
public:
    VideoFrame();
    ~VideoFrame();
    /**
    * Verifies that current allocated buffer size is larger than or equal to the input size.
    * If the current buffer size is smaller, a new allocation is made and the old buffer data
    * is copied to the new buffer.
    * Buffer size is updated to minimumSize.
    */
    int VerifyAndAllocate(const int minimumSize);
    /**
    *    Update length of data buffer in frame. Function verifies that new length is less or
    *    equal to allocated size.
    */
    int SetLength(const int newLength);
    /*
    *    Swap buffer and size data
    */
    int Swap(unsigned char*& newMemory,
                       int& newLength,
                       int& newSize);
    /*
    *    Swap buffer and size data
    */
    int SwapFrame(VideoFrame* videoFrame); ///int SwapFrame(VideoFrame& videoFrame);
    /**
    *    Copy buffer: If newLength is bigger than allocated size, a new buffer of size length
    *    is allocated.
    */
    int CopyFrame(VideoFrame* videoFrame); ///int CopyFrame(const VideoFrame& videoFrame);
    /**
    *    Copy buffer: If newLength is bigger than allocated size, a new buffer of size length
    *    is allocated.
    */
    int CopyFrame(int length, unsigned char* sourceBuffer);
    /**
    *    Delete VideoFrame and resets members to zero
    */
    void Free();
    /**
    *   Set frame timestamp (90kHz)
    */
    void SetTimeStamp(const int timeStamp) {_timeStamp = timeStamp;}
    /**
    *   Get pointer to frame buffer
    */
    unsigned char*    Buffer() {return _buffer;}

    //unsigned char*&   Buffer() {return _buffer;}

    /**
    *   Get allocated buffer size
    */
    int    Size() const {return _bufferSize;}
    /**
    *   Get frame length
    */
    int    Length() const {return _bufferLength;}
    /**
    *   Get frame timestamp (90kHz)
    */
    int    TimeStamp() const {return _timeStamp;}
    /**
    *   Get frame width
    */
    int    Width() const {return _width;}
    /**
    *   Get frame height
    */
    int    Height() const {return _height;}
    /**
    *   Set frame width
    */
    void   SetWidth(const int width)  {_width = width;}
    /**
    *   Set frame height
    */
    void  SetHeight(const int height) {_height = height;}
    /**
    *   Set render time in miliseconds
    */
    void SetRenderTime(const long long renderTimeMs) {_renderTimeMs = renderTimeMs;}
    /**
    *  Get render time in miliseconds
    */
    long long    RenderTimeMs() const {return _renderTimeMs;}

	void CutWidth(int len);

private:
    void Set(unsigned char* buffer,
             int size,
             int length,
             int timeStamp);

    unsigned char*          _buffer;          // Pointer to frame buffer
    int          _bufferSize;      // Allocated buffer size
    int          _bufferLength;    // Length (in bytes) of buffer
    int          _timeStamp;       // Timestamp of frame (90kHz)
    int          _width;
    int          _height;
    long long           _renderTimeMs;
}; // end of VideoFrame class declaration

// inline implementation of VideoFrame class:
inline
VideoFrame::VideoFrame():
    _buffer(0),
    _bufferSize(0),
    _bufferLength(0),
    _timeStamp(0),
    _width(0),
    _height(0),
    _renderTimeMs(0)
{
    //
}
inline
VideoFrame::~VideoFrame()
{
    if(_buffer)
    {
        free(_buffer); ///delete [] _buffer;
        _buffer = NULL;
    }
}


inline
int
VideoFrame::VerifyAndAllocate(const int minimumSize)
{
    if (minimumSize < 1)
    {
        return -1;
    }
    if(minimumSize > _bufferSize)
    {
        // create buffer of sufficient size
        unsigned char* newBufferBuffer = (unsigned char*)malloc( sizeof(unsigned char)*minimumSize ); ///unsigned char* newBufferBuffer = new unsigned char[minimumSize];
		if(NULL==newBufferBuffer){
			return -1;
		}
		if(_buffer)
        {
            // copy old data
            memcpy(newBufferBuffer, _buffer, _bufferSize);
            free( _buffer ); ///delete [] _buffer;
        }
        else
        {
            memset(newBufferBuffer, 0, minimumSize * sizeof(unsigned char));
        }
        _buffer = newBufferBuffer;
        _bufferSize = minimumSize;
    }
    return 0;
}

inline
int
VideoFrame::SetLength(const int newLength)
{
    if (newLength >_bufferSize )
    { // can't accomodate new value
        return -1;
    }
     _bufferLength = newLength;
     return 0;
}

inline
int
VideoFrame::SwapFrame(VideoFrame* videoFrame) ///VideoFrame::SwapFrame(VideoFrame& videoFrame)
{
    int tmpTimeStamp  = _timeStamp;
    int tmpWidth      = _width;
    int tmpHeight     = _height;
    long long  tmpRenderTime = _renderTimeMs;
	/*
    _timeStamp = videoFrame._timeStamp;
    _width = videoFrame._width;
    _height = videoFrame._height;
    _renderTimeMs = videoFrame._renderTimeMs;

    videoFrame._timeStamp = tmpTimeStamp;
    videoFrame._width = tmpWidth;
    videoFrame._height = tmpHeight;
    videoFrame._renderTimeMs = tmpRenderTime;

    return Swap(videoFrame._buffer, videoFrame._bufferLength, videoFrame._bufferSize);
    */
    _timeStamp = videoFrame->_timeStamp;
    _width = videoFrame->_width;
    _height = videoFrame->_height;
    _renderTimeMs = videoFrame->_renderTimeMs;

    videoFrame->_timeStamp = tmpTimeStamp;
    videoFrame->_width = tmpWidth;
    videoFrame->_height = tmpHeight;
    videoFrame->_renderTimeMs = tmpRenderTime;

    return Swap(videoFrame->_buffer, videoFrame->_bufferLength, videoFrame->_bufferSize);
}

inline
int
VideoFrame::Swap(unsigned char*& newMemory, int& newLength, int& newSize)
{
    unsigned char* tmpBuffer = _buffer;
    int tmpLength = _bufferLength;
    int tmpSize = _bufferSize;
    _buffer = newMemory;
    _bufferLength = newLength;
    _bufferSize = newSize;
    newMemory = tmpBuffer;
    newLength = tmpLength;
    newSize = tmpSize;
    return 0;
}

inline
int
VideoFrame::CopyFrame(int length, unsigned char* sourceBuffer)
{
    if (length > _bufferSize)
    {
        int ret = VerifyAndAllocate(length);
        if (ret < 0)
        {
            return ret;
        }
    }
     memcpy(_buffer, sourceBuffer, length);
    _bufferLength = length;
    return 0;
}

inline
int
VideoFrame::CopyFrame(VideoFrame* videoFrame) ///VideoFrame::CopyFrame(const VideoFrame& videoFrame)
{
    if(CopyFrame(videoFrame->Length(), videoFrame->Buffer()) != 0)
    {
        return -1;
    }
    _timeStamp = videoFrame->_timeStamp;
    _width = videoFrame->_width;
    _height = videoFrame->_height;
    _renderTimeMs = videoFrame->_renderTimeMs;
    return 0;
}

inline
void
VideoFrame::Free()
{
    _timeStamp = 0;
    _bufferLength = 0;
    _bufferSize = 0;
    _height = 0;
    _width = 0;
    _renderTimeMs = 0;

    if(_buffer)
    {
        free( _buffer ); ///delete [] _buffer;
        _buffer = NULL;
    }
}

inline
void 
VideoFrame::CutWidth(int len)
{
	unsigned char*src = _buffer;
	int width = _width;
	int height = _height;
	int cut = len;
	 ///Y
        unsigned char* p = src+width-cut;
        for(int i=1;i<height;i++){
                for(int j=0;j<width-cut;j++){
                        *p = *(p+i*cut);
                        p++;
                }
        }
        int cut_cnt = width*height;
        ///U
        for(int i=0;i<height/2;i++){
                for(int j=0;j<(width-cut)/2;j++){
                        *p = *(src+(i*width)/2+cut_cnt+j);
                        p++;
                }
        }
        ///V
        cut_cnt = width*height*5/4;
        for(int i=0;i<height/2;i++){
                for(int j=0;j<(width-cut)/2;j++){
                        *p = *(src+(i*width)/2+cut_cnt+j);
                        p++;
                }
        }
	_width = _width - cut;
	_bufferLength = _width*_height*3/2;
}


#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_INTERFACE_VIDEO_RENDER_DEFINES_H_
