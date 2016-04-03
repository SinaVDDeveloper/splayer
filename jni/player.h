#ifndef __FFPLAY__H__
#define __FFPLAY__H__
#include "config.h" 
#include <jni.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h> 
#include <stdio.h> 
#include <stdarg.h> 
#include <assert.h>

#include "player_helper.h"
#include "video_render_android.h"
#include "player_jni_entry.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/avstring.h>
#include <libavutil/colorspace.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libavutil/parseutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/avassert.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavcodec/avfft.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
}
#endif



/*****************************mediaplayer event define***************************************/
//msg code
#define MEDIA_NOP 					0  
#define MEDIA_PREPARED  			1	 
#define MEDIA_PLAYBACK_COMPLETE 	2	  
#define MEDIA_BUFFERING_UPDATE 		3	  
#define MEDIA_SEEK_COMPLETE 		4    
#define MEDIA_SET_VIDEO_SIZE 		5	 	  
#define MEDIA_ERROR 				100	 
#define MEDIA_INFO 					200
//ERROR what
#define MEDIA_ERROR_UNKNOWN 		1
#define MEDIA_ERROR_SERVER_DIED 	100
#define MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK 	200
#define MEDIA_ERROR_IO 				-1004 // File or network related operation errors. 
#define MEDIA_ERROR_MALFORMED 		-1007 // Bitstream is not conforming to the related coding standard or file spec. 
#define MEDIA_ERROR_UNSUPPORTED 	-1010 // Bitstream is conforming to the related coding standard or file spec, but the media framework does not support the feature.
#define MEDIA_ERROR_TIMED_OUT 		-110  // Some operation takes too long to complete, usually more than 3-5 seconds.
#define MEDIA_ERROR_DECODER_FAIL 	-2000 //add for hardware decoder error
//INFO what
#define MEDIA_INFO_UNKNOWN 				1
#define MEDIA_INFO_VIDEO_TRACK_LAGGING	700
#define MEDIA_INFO_BUFFERING_START		701
#define MEDIA_INFO_BUFFERING_END		702
#define MEDIA_INFO_BAD_INTERLEAVING		800
#define MEDIA_INFO_NOT_SEEKABLE			801
#define MEDIA_INFO_METADATA_UPDATE 		802


/*****************************internal define*********************************************/

#define	SPLAYER_SUCCESS					0
#define	SPLAYER_ERROR_NORMAL			-1
#define	SPLAYER_ERROR_PARAMETER 		-2
#define	SPLAYER_ERROR_NO_MEMORY 		-3
#define	SPLAYER_ERROR_SPALYER_STATE 	-4
#define	SPLAYER_ERROR_NETWORK 			-5
#define	SPLAYER_ERROR_UNSUPPORTED 		-6
#define	SPLAYER_ERROR_TIMEDOUT 			-7
#define	SPLAYER_ERROR_INTERNAL 			-8

enum splayer_state{
	SPALYER_STATE_ERROR = -1,
	SPALYER_STATE_IDLE = 0,
	SPALYER_STATE_INIT ,
	SPALYER_STATE_PREPARED ,
	SPALYER_STATE_PREPARING ,
	SPALYER_STATE_STARTED,
	SPALYER_STATE_PAUSED,
	SPALYER_STATE_STOPPED,
	SPALYER_STATE_COMPLETED,
	SPALYER_STATE_END
};

enum video_scaling_mode{
	VIDEO_SCALING_MODE_SCALE_TO_FIT = 0, //按比例缩放
	VIDEO_SCALING_MODE_FULL_SCREEN,  //全屏占满
	VIDEO_SCALING_MODE_ORIGINAL  //原始尺寸
};


//#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
//static int MAX_FRAMES = 100; //#define MIN_FRAMES 100 ///#define MIN_FRAMES 5   ///for buffer debug
//static int MIN_FRAMES = 5; 

/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
#define SDL_AUDIO_BUFFER_SIZE  160*3*2 ///1024

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.01
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000


#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 4

#define BPP 1

#define FRAME_DROP_STEP_1 1
#define FRAME_DROP_STEP_2 2
//#define FRAME_DROP_STEP_3 3

#define FRAME_DROP_THRESHOLD 5 //100



//for hard 
#ifndef ANDROID_DECODER_START_CMD
#define ANDROID_DECODER_START_CMD 			1  
#define ANDROID_SURFACE_CREATE_CMD 			2
#define ANDROID_SURFACE_DESTROY_CMD 		4
#define ANDROID_SET_JVM_CMD 				8  
#define ANDROID_SET_MCLASS_CMD 				16  
#define ANDROID_SET_SURFACE_CMD 			32
#define ANDROID_DECODER_FLUSH_CMD 			48
#endif


typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList *next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    int serial;
    LYH_Mutex *mutex;
    LYH_Cond *cond;
} PacketQueue;


typedef struct VideoPicture {
    double pts;             // presentation timestamp for this picture
    int64_t pos;            // byte position in file
    LYH_Overlay *bmp; ///SDL_Overlay *bmp;
    int width, height; /* source height & width */
    int allocated;
    int reallocate;
    int serial;  ///

    AVRational sar;
} VideoPicture;

typedef struct SubPicture {
    double pts; /* presentation time stamp for this picture */
    AVSubtitle sub;
    int serial;
} SubPicture;

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout; //left right center ,  voice channel layout
    enum AVSampleFormat fmt;
} AudioParams;

typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */ /// pts - last_updated (time)
    double last_updated;  ///last update time
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

enum ShowMode {
    SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
};

//for hard
struct decoder_cmd{
	int type_;
	void* data_;
};

//release 版本1 debug版本为0
#define RELEASE_DEBUG_LOG_LEVEL ((1==1)?AV_LOG_DEBUG:AV_LOG_ERROR)

class MediaPlayer {
public:
	static MediaPlayer* Create(int use_hard_decode);
	virtual ~MediaPlayer() {};
	static void Destroy(MediaPlayer* mp);
	virtual int using_hard_decode(void) = 0 ;
	virtual int setup(JavaVM* jvm, jobject thiz, jobject weak_this) = 0;
	virtual int set_surfaceview(JavaVM* jvm,jobject jsurfaceview) = 0;
	virtual int set_surface(JavaVM* jvm, void* surface) = 0; //add for hard
	virtual int set_video_scaling_mode(int mode) = 0;
	virtual int set_datasource(const char* path) = 0;
	virtual int set_looping(int loop) = 0;
	virtual int prepare(void) = 0;
	virtual int prepareAsync(void) = 0;
	virtual int start(void) = 0;
	virtual int pause(void) = 0;
	virtual int stop(void) = 0;
	virtual int reset(void) = 0;
	virtual int release(void) = 0;
	virtual int seekto(int msec) = 0;
	virtual int is_playing(void) = 0;
	virtual int get_video_width(void) = 0;
	virtual int get_video_height(void) = 0;
	virtual float get_video_aspect_ratio(void) = 0;
	virtual int get_current_position(void) = 0;
	virtual int get_duration(void) = 0;
	virtual int is_looping(void) = 0;
	virtual int is_buffering(void) = 0;
	virtual int config(const char* cfg , int val) = 0;
	virtual int setVolume(float left, float right) = 0;	
	virtual void notify(int msg, int ext1, int ext2, void *obj) = 0;

} ;


/****************************soft intrface*************************************/
class MediaPlayerSoft: public MediaPlayer{
public:
	explicit MediaPlayerSoft();  
	virtual ~MediaPlayerSoft();
	virtual int using_hard_decode(void);
	virtual int setup(JavaVM* jvm, jobject thiz, jobject weak_this);
	virtual int set_surfaceview(JavaVM* jvm,jobject jsurfaceview);
	virtual int set_surface(JavaVM* jvm, void* surface);
	virtual int set_video_scaling_mode(int mode);
	virtual int set_datasource(const char* path);
	virtual int set_looping(int loop);
	virtual int prepare(void);
	virtual int prepareAsync(void);
	virtual int start(void);
	virtual int pause(void);
	virtual int stop(void);
	virtual int reset(void);
	virtual int release(void);
	virtual int seekto(int msec);
	virtual int is_playing(void);
	virtual int get_video_width(void);
	virtual int get_video_height(void);
	virtual float get_video_aspect_ratio(void);
	virtual int get_current_position(void);
	virtual int get_duration(void);
	virtual int is_looping(void);
	virtual int is_buffering(void);
	virtual int config(const char* cfg , int val);
	virtual int setVolume(float left, float right); 
	virtual void notify(int msg, int ext1, int ext2, void *obj);

private:
	int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
	int packet_queue_put(PacketQueue *q, AVPacket *pkt);
	int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
	void packet_queue_init(PacketQueue *q);
	void packet_queue_flush(PacketQueue *q);
	void packet_queue_flush_half(PacketQueue *q);
	void packet_queue_destroy(PacketQueue *q);
	void packet_queue_abort(PacketQueue *q);
	void packet_queue_start(PacketQueue *q);
	int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);
	void blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh);
	void free_picture(VideoPicture *vp);
	void free_subpicture(SubPicture *sp);
	void video_image_display();
	void video_redisplay();
	void stream_close();
	void do_exit(int destroy_vs);
	int video_open(int force_set_video_mode, VideoPicture *vp);
	void video_display();
	double get_clock(Clock *c);
	void set_clock_at(Clock *c, double pts, int serial, double time);
	void set_clock(Clock *c, double pts, int serial);
	void set_clock_speed(Clock *c, double speed);
	void init_clock(Clock *c, int *queue_serial);
	void sync_clock_to_slave(Clock *c, Clock *slave);
	int get_master_sync_type();
	double get_master_clock();
	void stream_seek(int64_t pos, int64_t rel, int seek_by_bytes);
	void stream_toggle_pause(int unpause); //unpause 1强制开始 0 反转状态
	void toggle_pause();
	void step_to_next_frame();
	double compute_target_delay(double delay);
	void pictq_next_picture();
	int pictq_prev_picture() ;
	void update_video_pts(double pts, int64_t pos, int serial);
	void video_refresh(double *remaining_time);
	int alloc_picture();
	int queue_picture(AVFrame *src_frame, double pts, int64_t pos, int serial);
	int get_video_frame(AVFrame *frame, AVPacket *pkt, int *serial);
	int synchronize_audio(int nb_samples);
	int audio_decode_frame();
	int audio_open(int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
	int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);
	AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id, AVFormatContext *s, AVStream *st, AVCodec *codec);
	int stream_component_open(int stream_index);
	void stream_component_close(int stream_index);
	int is_realtime(AVFormatContext *s);
	int is_seekable(AVFormatContext *s);
	AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,AVDictionary *codec_opts);
	int videostate_init(AVInputFormat *iformat);
	int do_stream_open();
	int stream_start();
	int stream_open(AVInputFormat *iformat);
	void refresh_loop_wait_event(LYH_event *event);
	int seek_to(int msec);
	void event_loop();
public:
	static int lockmgr(void **mtx, enum AVLockOp op);
	static void* video_thread(void *arg);
	static void* subtitle_thread(void *arg);
	static void sdl_audio_callback(void *opaque, char *stream, int len);
	static int decode_interrupt_cb(void *ctx);
	static void* read_thread(void *arg);
	static void* event_thread(void *arg);
	static void* stream_open_thread(void *arg);
	static void player_log_callback_help(void *ptr, int level, const char *fmt, va_list vl);
private:
    LYH_Thread read_tid;
    LYH_Thread video_tid;
    AVInputFormat *iformat;
    int abort_request;
    int force_refresh;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    int realtime;
    int audio_finished;
    int video_finished;

    Clock audclk;
    Clock vidclk;
    ///Clock extclk;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
	double audio_clock_total; //m3u8时使用这个总clock
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t silence_buf[SDL_AUDIO_BUFFER_SIZE];
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    int audio_buf_frames_pending;
    AVPacket audio_pkt_temp;
    AVPacket audio_pkt;
    int audio_pkt_temp_serial;
    int audio_last_serial;
    struct AudioParams audio_src; ///文件的声音参数?
    struct AudioParams audio_tgt; ///输出到硬件的声音参数
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;
    AVFrame *frame;
    int64_t audio_frame_next_pts;
    enum ShowMode show_mode;
    LYH_Thread subtitle_tid;
    int subtitle_stream;
    AVStream *subtitle_st;
    PacketQueue subtitleq;
    SubPicture subpq[SUBPICTURE_QUEUE_SIZE];
    int subpq_size, subpq_rindex, subpq_windex;
    LYH_Mutex *subpq_mutex;
    LYH_Cond *subpq_cond;

    double frame_timer;  ///视频帧同步参考基准时间
    double frame_last_pts;
    double frame_last_duration;
    double frame_last_dropped_pts;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int64_t frame_last_dropped_pos;
    int frame_last_dropped_serial;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    int64_t video_current_pos;      // current displayed file pos
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    LYH_Mutex *pictq_mutex;
    LYH_Cond *pictq_cond;

    struct SwsContext *img_convert_ctx;

    LYH_Rect last_display_rect;

    char *filename; //char filename[1024];
    int width, height, xleft, ytop; ///screen->w;
    int frame_width, frame_height;
    int step;

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    LYH_Cond *continue_read_thread;

	int wanted_stream[AVMEDIA_TYPE_NB];

	int audio_disable;
	int video_disable;
	int subtitle_disable;
	int genpts;
	AVPacket flush_pkt;

	AVDictionary *format_opts;
	AVDictionary *codec_opts;
	AVDictionary *resample_opts;
	
	LYH_Surface *screen;
	LYH_Thread event_thread_id;

	JavaVM* 	jvm;
	int buffer_full;
	enum splayer_state state;
	enum video_scaling_mode scaling_mode;
	int looping;
	int seek_msec;  // seek second
	
	int fast;
	int lowres;
	int loop;   //play how many loops
	int framedrop;
	int skip_frame;
	int skip_idct;
	int skip_loopfilter;
	int skip_auto_closed;  ///自动跳帧0 打开 1 关闭

	LYH_Context* hardware_context;
	int64_t duration;
	int laset_buffer_percent;
	int MAX_QUEUE_SIZE; //缓冲区最大字节数
	int MAX_FRAMES;    //缓冲区最大包数
	JNIMediaPlayerListener* mediaplayer_listener;
	LYH_Mutex *mp_listener_lock;
	int eof;
	LYH_Thread stream_open_thread_id;
	JNIAndroidOpenGl2RendererListener* opengl2_render_listener;
	int last_notify_msg;
	int send_seek_complete_notify;
	int seeking;
	int frame_drops_early_continue;
	int frame_drops_late_continue;
	int frame_drop_step;
	double last_frame_drop_step_1_time;
	int frame_drop_step_switch_1_count;
	int frame_drop_step_1_disable;
	double last_frame_drop_step_0_time;
	int frame_drop_step_switch_0_count;
	int frame_drop_step_0_disable;

	//LYH_Mutex *pause_lock;
	//add for hrad
	//int hard_decode_mode;  //soft or hard, default 0 soft
	//JNIAndroidSurfaceRendererListener* surface_render_listener;//void* surface; 
	//AVFrame *video_decode_frame; //硬解出来的frame没有实际数据只有PTS
	//int 	video_decode_frame_show;
};


/****************************hard intrface*************************************/


class MediaPlayerHard: public MediaPlayer{
public:
	explicit MediaPlayerHard();  
	virtual ~MediaPlayerHard();
	virtual int using_hard_decode(void);
	virtual int setup(JavaVM* jvm, jobject thiz, jobject weak_this);
	virtual int set_surfaceview(JavaVM* jvm,jobject jsurfaceview);
	virtual int set_surface(JavaVM* jvm, void* surface);
	virtual int set_video_scaling_mode(int mode);
	virtual int set_datasource(const char* path);
	virtual int set_looping(int loop);
	virtual int prepare(void);
	virtual int prepareAsync(void);
	virtual int start(void);
	virtual int pause(void);
	virtual int stop(void);
	virtual int reset(void);
	virtual int release(void);
	virtual int seekto(int msec);
	virtual int is_playing(void);
	virtual int get_video_width(void);
	virtual int get_video_height(void);
	virtual float get_video_aspect_ratio(void);
	virtual int get_current_position(void);
	virtual int get_duration(void);
	virtual int is_looping(void);
	virtual int is_buffering(void);
	virtual int config(const char* cfg , int val);
	virtual int setVolume(float left, float right); 
	virtual void notify(int msg, int ext1, int ext2, void *obj);
	
private:
	int send_decoder_cmd( AVCodecContext *conetxt, int ctype, void* cdata);
	int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
	int packet_queue_put(PacketQueue *q, AVPacket *pkt);
	int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
	void packet_queue_init(PacketQueue *q);
	void packet_queue_flush(PacketQueue *q);
	void packet_queue_destroy(PacketQueue *q);
	void packet_queue_abort(PacketQueue *q);
	void packet_queue_start(PacketQueue *q);
	int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);
	void blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh);
	void free_picture(VideoPicture *vp);
	void free_subpicture(SubPicture *sp);
	void video_image_display();
	void video_redisplay();
	void stream_close();
	void do_exit(int destroy_vs);
	int video_open(int force_set_video_mode, VideoPicture *vp);
	void video_display();
	double get_clock(Clock *c);
	void set_clock_at(Clock *c, double pts, int serial, double time);
	void set_clock(Clock *c, double pts, int serial);
	void set_clock_speed(Clock *c, double speed);
	void init_clock(Clock *c, int *queue_serial);
	void sync_clock_to_slave(Clock *c, Clock *slave);
	int get_master_sync_type() ;
	double get_master_clock();
	void stream_seek(int64_t pos, int64_t rel, int seek_by_bytes);
	void stream_toggle_pause(int unpause); //unpause 1强制开始 0 反转状态
	void toggle_pause();
	void step_to_next_frame();
	double compute_target_delay(double delay);
	void pictq_next_picture();
	int pictq_prev_picture();
	void update_video_pts(double pts, int64_t pos, int serial);
	void video_refresh(double *remaining_time);
	int alloc_picture();
	int queue_picture(AVFrame *src_frame, double pts, int64_t pos, int serial);
	int get_video_frame(AVFrame *frame, AVPacket *pkt, int *serial);
	int synchronize_audio(int nb_samples);
	int audio_decode_frame();
	int audio_open(int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
	int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);
	AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,AVFormatContext *s, AVStream *st, AVCodec *codec);
	int stream_component_open(int stream_index);
	void stream_component_close(int stream_index);
	int is_realtime(AVFormatContext *s);
	int is_seekable(AVFormatContext *s);
	AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,AVDictionary *codec_opts);
	int videostate_init(AVInputFormat *iformat);
	int do_stream_open( );
	int stream_start();
	int stream_open(AVInputFormat *iformat);
	void refresh_loop_wait_event(LYH_event *event);
	int seek_to(int msec);
	void event_loop();
public:
	static int lockmgr(void **mtx, enum AVLockOp op);
	static void* video_thread(void *arg);
	static void* subtitle_thread(void *arg);
	static void sdl_audio_callback(void *opaque, char *stream, int len);
	static int decode_interrupt_cb(void *ctx);
	static void* read_thread(void *arg);
	static void* event_thread(void *arg);
	static void* stream_open_thread(void *arg);
	static void player_log_callback_help(void *ptr, int level, const char *fmt, va_list vl);

private:
    LYH_Thread read_tid;
    LYH_Thread video_tid;
    AVInputFormat *iformat;
    int abort_request;
    int force_refresh;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    int realtime;
    int audio_finished;
    int video_finished;

    Clock audclk;
    Clock vidclk;
    ///Clock extclk;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
	double audio_clock_total; //m3u8时使用这个总clock
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t silence_buf[SDL_AUDIO_BUFFER_SIZE];
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    int audio_buf_frames_pending;
    AVPacket audio_pkt_temp;
    AVPacket audio_pkt;
    int audio_pkt_temp_serial;
    int audio_last_serial;
    struct AudioParams audio_src; ///文件的声音参数?
    struct AudioParams audio_tgt; ///输出到硬件的声音参数
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;
    AVFrame *frame;
    int64_t audio_frame_next_pts;
    enum ShowMode show_mode;
    LYH_Thread subtitle_tid;
    int subtitle_stream;
    AVStream *subtitle_st;
    PacketQueue subtitleq;
    SubPicture subpq[SUBPICTURE_QUEUE_SIZE];
    int subpq_size, subpq_rindex, subpq_windex;
    LYH_Mutex *subpq_mutex;
    LYH_Cond *subpq_cond;

    double frame_timer;  ///视频帧同步参考基准时间
    double frame_last_pts;
	int frame_last_serial;  //为解决拖动伪直播有时会卡BUG
    double frame_last_duration;
    double frame_last_dropped_pts;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int64_t frame_last_dropped_pos;
    int frame_last_dropped_serial;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    int64_t video_current_pos;      // current displayed file pos
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    LYH_Mutex *pictq_mutex;
    LYH_Cond *pictq_cond;

    struct SwsContext *img_convert_ctx;

    LYH_Rect last_display_rect;

    char *filename; //char filename[1024];
    int width, height, xleft, ytop; ///screen->w;
    int frame_width, frame_height;
    int step;

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    LYH_Cond *continue_read_thread;

	int wanted_stream[AVMEDIA_TYPE_NB];

	int audio_disable;
	int video_disable;
	int subtitle_disable;
	int genpts;
	AVPacket flush_pkt;

	AVDictionary *format_opts;
	AVDictionary *codec_opts;
	AVDictionary *resample_opts;
	
	LYH_Surface *screen;
	LYH_Thread event_thread_id;

	JavaVM* 	jvm;
	int buffer_full;
	enum splayer_state state;
	enum video_scaling_mode scaling_mode;
	int looping;
	int seek_msec;  // seek second
	
	int fast;
	int lowres;
	int loop;   //play how many loops
	int framedrop;
	int skip_frame;
	int skip_idct;
	int skip_loopfilter;
	int skip_auto_closed;  ///自动跳帧0 打开 1 关闭

	LYH_Context* hardware_context;
	int64_t duration;
	int laset_buffer_percent;
	int MAX_QUEUE_SIZE;  //缓冲区最大字节数
	int MAX_FRAMES; //缓冲区最大包数
	JNIMediaPlayerListener* mediaplayer_listener;
	LYH_Mutex *mp_listener_lock;
	int eof;
	LYH_Thread stream_open_thread_id;
	JNIAndroidOpenGl2RendererListener* opengl2_render_listener;
	int last_notify_msg;
	int send_seek_complete_notify;
	int seeking;
	int frame_drops_early_continue;
	int frame_drops_late_continue;
	int frame_drop_step;
	//add for hrad
	//int hard_decode_mode;  //soft or hard, default 0 soft
	JNIAndroidSurfaceRendererListener* surface_render_listener;//void* surface; 
	AVFrame *video_decode_frame; //硬解出来的frame没有实际数据只有PTS
	int 	video_decode_frame_show;

};


#endif 
