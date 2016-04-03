#include <new>
#include <map>
#include <list>
#include <jni.h>
#include <pthread.h>
#include <unistd.h>

extern "C" {
#include "avcodec.h"
#include "libavutil/imgutils.h"
#include "libavutil/intreadwrite.h"
#include "internal.h"
#include "get_bits.h"
#include "golomb.h"
}

#define INFO_TRY_AGAIN_LATER -1
#define INFO_OUTPUT_FORMAT_CHANGED -2
#define INFO_OUTPUT_BUFFERS_CHANGED -3
#define ERROR_END_OF_STREAM -4
#ifndef OK
#define OK 0
#endif


#define ANDROID_DECODER_START_CMD 			1  
#define ANDROID_SURFACE_CREATE_CMD 			2
#define ANDROID_SURFACE_DESTROY_CMD 		4
#define ANDROID_SET_JVM_CMD 				8  
#define ANDROID_SET_MCLASS_CMD 				16  
#define ANDROID_SET_SURFACE_CMD 			32
#define ANDROID_DECODER_FLUSH_CMD 			48  



#define INPUT_QUEUE_LEN			10  //10
#define OUTPUT_QUEUE_LEN		5  //10


//#define OMX_QCOM_COLOR_FormatYVU420SemiPlanar 0x7FA30C00
struct cmd_data{
	int type;
	void* data;
};

struct Frame {
    int status;
    size_t size;
    int64_t time;
    int key;
    uint8_t *buffer; //in
    AVFrame *vframe; //out
};

struct TimeStamp {
    int64_t pts;
    int64_t reordered_opaque;
	int64_t pos;  //avpacket,
	int serial; //avpacket, for seek flush old packet 
	int duration;
};

enum decoder_state{
	decoder_state_null = 0,
	decoder_state_start,
	decoder_state_release, 
};

enum decoder_type{
	DT_UNKNOW_DECODER = 0,
	DT_MTK_DECODER = 1
};

enum frame_pts_order{
	FRAME_PTS_ORDER_UNDETECT = 0,
	FRAME_PTS_ORDER_DISORDER = 1,
	FRAME_PTS_ORDER_PROPER = 2
};


struct StagefrightContext {
    AVCodecContext *avctx;
    AVBitStreamFilterContext *bsfc;
    uint8_t* orig_extradata;
    int orig_extradata_size;
    std::list<Frame*> *in_queue, *out_queue;
    pthread_mutex_t in_mutex, out_mutex, time_map_mutex;
    pthread_cond_t condition;
    pthread_t decode_thread_id;

    Frame *end_frame;
    bool source_done;
    volatile  int thread_started, thread_exited, start_decode,stop_decode; 

    AVFrame *prev_frame;
    std::map<int64_t, TimeStamp> *ts_map;
    int64_t frame_index;

    uint8_t *dummy_buf;
    int dummy_bufsize;

    char *decoder_component;
	decoder_type dtype;
	frame_pts_order pts_order;
	int64_t last_frame_pts;
	int last_frame_serial;
	int frame_order_detect_cout;
	int mp4_h264;
	GetBitContext gb;

	///
	JavaVM *jvm;
	jclass mediacodec_class;
	jmethodID mediacodec_create;
	jmethodID mediacodec_start; 
	jmethodID mediacodec_stop; 
	jmethodID mediacodec_alloc;
	jmethodID mediacodec_decode;
	jmethodID mediacodec_pts;
	jmethodID mediacodec_registerContext;//jfieldID mNativeContext;
	//jmethodID mediacodec_flush;
	jfieldID csd0ArrayID;
	jfieldID csd1ArrayID;
	jfieldID encodeFrameID;
	jfieldID decodeFrameID;
	//jfieldID mCodecNameID;
	jobject mediacodec_obj;
	jobject mediacodec_surface;
	long	decodeFramePTS;
	int width;
	int height;
	decoder_state state;
	uint8_t *csd0; 
	uint8_t* csd1;
	int csd0_len;
	int csd1_len;
	int surface_cmd;
	//int flush_cmd;
	//mtk pts 
	//int64_t videoframe_pts_base; //(the frame with min pts)
	//int64_t videoframe_pos_base; //(first frame's pos)
	//int videoframe_serial_last;
	//int64_t videoframe_pos_last; // (new pts)
	//int64_t videoframe_pts_last; // (new pos)
	//int64_t videoframe_base_index;
	///
	///int error;
	
};


//private ByteBuffer decodeFrame; ->  avframe
struct DecodeBuffer{
	uint8_t* data;
	int data_len;
	int size;
	jlong pts;
};



typedef struct H264SPS {
    int profile_idc;
    int level_idc;
    int chroma_format_idc;
    int transform_bypass;              ///< qpprime_y_zero_transform_bypass_flag
    int log2_max_frame_num;            ///< log2_max_frame_num_minus4 + 4
    int poc_type;                      ///< pic_order_cnt_type
    int log2_max_poc_lsb;              ///< log2_max_pic_order_cnt_lsb_minus4
    int delta_pic_order_always_zero_flag;
    int offset_for_non_ref_pic;
    int offset_for_top_to_bottom_field;
    int poc_cycle_length;              ///< num_ref_frames_in_pic_order_cnt_cycle
    int ref_frame_count;               ///< num_ref_frames
    int gaps_in_frame_num_allowed_flag;
    int mb_width;                      ///< pic_width_in_mbs_minus1 + 1
    int mb_height;                     ///< pic_height_in_map_units_minus1 + 1
    int frame_mbs_only_flag;
    int mb_aff;                        ///< mb_adaptive_frame_field_flag
    int direct_8x8_inference_flag;
    int crop;                          ///< frame_cropping_flag

    /* those 4 are already in luma samples */
    unsigned int crop_left;            ///< frame_cropping_rect_left_offset
    unsigned int crop_right;           ///< frame_cropping_rect_right_offset
    unsigned int crop_top;             ///< frame_cropping_rect_top_offset
    unsigned int crop_bottom;          ///< frame_cropping_rect_bottom_offset
    int vui_parameters_present_flag;
    AVRational sar;
    int video_signal_type_present_flag;
    int full_range;
    int colour_description_present_flag;
    enum AVColorPrimaries color_primaries;
    enum AVColorTransferCharacteristic color_trc;
    enum AVColorSpace colorspace;
    int timing_info_present_flag;
    uint32_t num_units_in_tick;
    uint32_t time_scale;
    int fixed_frame_rate_flag;
    short offset_for_ref_frame[256]; // FIXME dyn aloc?
    int bitstream_restriction_flag;
    int num_reorder_frames;
    int scaling_matrix_present;
    uint8_t scaling_matrix4[6][16];
    uint8_t scaling_matrix8[6][64];
    int nal_hrd_parameters_present_flag;
    int vcl_hrd_parameters_present_flag;
    int pic_struct_present_flag;
    int time_offset_length;
    int cpb_cnt;                          ///< See H.264 E.1.2
    int initial_cpb_removal_delay_length; ///< initial_cpb_removal_delay_length_minus1 + 1
    int cpb_removal_delay_length;         ///< cpb_removal_delay_length_minus1 + 1
    int dpb_output_delay_length;          ///< dpb_output_delay_length_minus1 + 1
    int bit_depth_luma;                   ///< bit_depth_luma_minus8 + 8
    int bit_depth_chroma;                 ///< bit_depth_chroma_minus8 + 8
    int residual_color_transform_flag;    ///< residual_colour_transform_flag
    int constraint_set_flags;             ///< constraint_set[0-3]_flag
    int new_;                              ///< flag to keep track if the decoder context needs re-init due to changed SPS
} H264SPS;

//static DecodeBuffer deocde_buffer = {0,0,0,0};
static int JNICALL writeDecodeFrame(JNIEnv * env,jobject obj,jint nativeContext,jlong pts)
{
	av_log(NULL, AV_LOG_INFO, "%s: into\n",__FUNCTION__);

	StagefrightContext* stagefright_context = (StagefrightContext*)nativeContext;
	
	if(!stagefright_context){
		av_log(NULL, AV_LOG_ERROR, "%s: stagefright_context NULL\n",__FUNCTION__);
		return -1;
	}
	stagefright_context->decodeFramePTS = pts;
	/*
	jobject decodeFrame = env->GetObjectField(mediacodec_obj, decodeFrameID);
	uint8_t *pdecodeFrame = (uint8_t *)env->GetDirectBufferAddress(decodeFrame);
	int buf_size = env->GetDirectBufferCapacity(decodeFrame);
	if(deocde_buffer.size < buf_size){
		if(deocde_buffer.data){
			free(deocde_buffer.data);
		}
		deocde_buffer.data = (uint8_t*)malloc(buf_size);
		if(NULL==deocde_buffer.data){
			av_log(NULL, AV_LOG_ERROR, "%s: malloc fail\n",__FUNCTION__);
			return -1;
		}
		deocde_buffer.size = buf_size ;
	}
	if(!deocde_buffer.data){
		av_log(NULL, AV_LOG_ERROR, "%s: data NULL\n",__FUNCTION__);
		return -1;
	}

	memcpy(deocde_buffer.data, pdecodeFrame, buf_size);
	deocde_buffer.data_len = buf_size;
	deocde_buffer.pts = pts;
	*/
	
	av_log(NULL, AV_LOG_INFO, "%s: out\n",__FUNCTION__);
	return 0;
	
}

static int isMTKDecoder(StagefrightContext *s)
{
	return s->dtype==DT_MTK_DECODER;
}

static bool compare_pts(const Frame*  first, const Frame*  second)
{
	if(first->status!=OK || second->status!=OK || first->vframe==NULL || second->vframe==NULL){
		return true;
	}
	
  	return ( first->vframe->pts < second->vframe->pts );
}

static frame_pts_order detect_frame_pts_order(StagefrightContext *s,Frame* frame)
{
	if(!frame || !frame->vframe){
		return FRAME_PTS_ORDER_UNDETECT;
	}

	if(s->last_frame_pts == -1){
		s->frame_order_detect_cout = 1;
		s->last_frame_pts = frame->vframe->pts;
		s->last_frame_serial = frame->vframe->display_picture_number;
		return FRAME_PTS_ORDER_UNDETECT;
	}
	if(s->last_frame_serial == frame->vframe->display_picture_number){
		if(s->last_frame_pts > frame->vframe->pts && 
			frame->vframe->pkt_duration!=0 &&
			(s->last_frame_pts-frame->vframe->pts)<2*OUTPUT_QUEUE_LEN*frame->vframe->pkt_duration){
			//av_log(NULL, AV_LOG_ERROR,"%s: disorder, s->last_frame_pts=%lld,frame->vframe->pts=%lld\n",__FUNCTION__,s->last_frame_pts,frame->vframe->pts);
			return FRAME_PTS_ORDER_DISORDER;
		}
		//else if(s->last_frame_pts < frame->vframe->pts){
		//	s->frame_order_detect_cout++;
		//	if(s->frame_order_detect_cout>=OUTPUT_QUEUE_LEN){
		//		av_log(NULL, AV_LOG_ERROR,"%s: proper, s->last_frame_pts=%lld,frame->vframe->pts=%lld\n",__FUNCTION__,s->last_frame_pts,frame->vframe->pts);
		//		return FRAME_PTS_ORDER_PROPER;	
		//	}
		//}
	}else{
		//av_log(NULL, AV_LOG_ERROR,"%s: frame serial not the same, s->last_frame_serial=%d,frame->vframe->display_picture_number=%d\n",__FUNCTION__,s->last_frame_serial,frame->vframe->display_picture_number);
	}
	
	s->last_frame_pts = frame->vframe->pts;
	s->last_frame_serial = frame->vframe->display_picture_number;
		
	return FRAME_PTS_ORDER_UNDETECT;
}
static bool have_diff_ts_frame(std::list<Frame*> *queue)
{
   	std::list<Frame*>::iterator it;
   	int64_t pos, pts;
  	if(queue->empty()){
        return false;
  	}
	Frame* f = queue->front();
	if(f==NULL || f->vframe==NULL){
		return true;
	}
   	pos = f->vframe->pkt_pos;
   	pts = f->vframe->pts;
    for (it=queue->begin(); it!=queue->end(); ++it){
		f = (*it);
		if(f->status!=OK){
			return true;
		}
		if(f->vframe==NULL){
			return true;
		}
        if(pts>f->vframe->pts && (pts-f->vframe->pts)>2*OUTPUT_QUEUE_LEN*f->vframe->pkt_duration){
        	av_log(NULL, AV_LOG_ERROR,"%s: find,base pts=%lld,pkt_pos=%lld, cur pts=%lld,pkt_pos=%lld\n",__FUNCTION__,pts,pos, f->vframe->pts,f->vframe->pkt_pos);
            return true;
        }else{
            pos = f->vframe->pkt_pos;
            pts = f->vframe->pts;
        }
    }
    return false;
}

static void* decode_thread(void *arg)
{
    AVCodecContext *avctx = (AVCodecContext*)arg;
    StagefrightContext *s = (StagefrightContext*)avctx->priv_data;
    Frame* frame;
    int decode_done = 0;
    int ret;
	//int32_t w, h;
	//const AVPixFmtDescriptor *pix_desc = av_pix_fmt_desc_get(avctx->pix_fmt);
    //int src_linesize[3];
    //const uint8_t *src_data[3];

	av_log(avctx, AV_LOG_DEBUG,"%s: decode thread into", __FUNCTION__);

	bool isAttached = false;
	JNIEnv* env = NULL;
	if (s->jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		ret = s->jvm->AttachCurrentThread(&env, NULL);
		if ((ret < 0) || !env) {
			av_log(avctx, AV_LOG_ERROR,"%s: Could not attach thread to JVM (%d, %p)", __FUNCTION__, ret, env);
		  	return NULL;
		}
		isAttached = true;
	}
			
    do {
SURFACE_CMD:
     	/*
     		if(s->surface_cmd == ANDROID_SURFACE_DESTROY_CMD){
			if(s->state != decoder_state_start ){
				av_log(avctx, AV_LOG_ERROR,"%s: decoder already destroy", __FUNCTION__);
			}else{
				av_log(avctx, AV_LOG_ERROR,"%s: ANDROID_SURFACE_DESTROY_CMD", __FUNCTION__);
				env->CallVoidMethod(s->mediacodec_obj, s->mediacodec_stop,(int)((void*)s));
				s->state = decoder_state_release;
			}
			s->surface_cmd = 0;
		}else if(s->surface_cmd == ANDROID_SURFACE_CREATE_CMD){
			if(s->state == decoder_state_start ){
				av_log(avctx, AV_LOG_ERROR,"%s: decoder already start", __FUNCTION__);
			}else{
				av_log(avctx, AV_LOG_ERROR,"%s: ANDROID_SURFACE_CREATE_CMD", __FUNCTION__);
				ret = env->CallIntMethod(s->mediacodec_obj, s->mediacodec_start,(int)((void*)s),avctx->width,avctx->height);
				if(0==ret){
					s->state = decoder_state_start;
				}
			}
			s->surface_cmd = 0;
		}
		*/
		
		while(s->state != decoder_state_start){
			av_log(avctx, AV_LOG_DEBUG,"%s: decoder not start now", __FUNCTION__);
			usleep(10000);
			if(decode_done || s->stop_decode){
				av_log(avctx, AV_LOG_DEBUG,"%s: decoder thread exit now", __FUNCTION__);
				frame = (Frame*)av_mallocz(sizeof(Frame));
        		if (!frame) {
            		frame         = s->end_frame;
            		frame->status = AVERROR(ENOMEM);
            		s->end_frame  = NULL;
        		}else{
					frame->status = ERROR_END_OF_STREAM;
				}
				goto push_frame;
			}
			if(0!=s->surface_cmd){
				av_log(avctx, AV_LOG_DEBUG,"%s: decoder thread exit now", __FUNCTION__);
				goto SURFACE_CMD;
			}
		}

		//if(s->flush_cmd){
		//	if(s->state==decoder_state_start){
		//		av_log(NULL, AV_LOG_ERROR,"%s: handler flush request,s->flush_cmd=%d", __FUNCTION__,s->flush_cmd);
		//		env->CallVoidMethod(s->mediacodec_obj, s->mediacodec_flush,(int)((void*)s),s->flush_cmd);
		//	}else{
		//		av_log(NULL, AV_LOG_ERROR,"%s: not flush for decoder not start yet", __FUNCTION__);
		//	}
		//	s->flush_cmd  = 0;
		//}
		ret = env->CallIntMethod(s->mediacodec_obj,s->mediacodec_decode,(int)((void*)s));        
        if (ret == OK) {
			frame = (Frame*)av_mallocz(sizeof(Frame));
        	if (!frame) {
            	frame         = s->end_frame;
            	frame->status = AVERROR(ENOMEM);
            	decode_done   = 1;
            	s->end_frame  = NULL;
            	goto push_frame;
        	}else{
				frame->status = ret;
			}
		
            frame->vframe = (AVFrame*)av_mallocz(sizeof(AVFrame));
            if (!frame->vframe) {
                frame->status = AVERROR(ENOMEM);
                decode_done   = 1;
                goto push_frame;
            }
			
            ///ret = ff_get_buffer(avctx, frame->vframe, AV_GET_BUFFER_FLAG_REF);
            ///if (ret < 0) {
            ///    frame->status = ret;
            ///    decode_done   = 1;
                //buffer->release();
            ///    goto push_frame;
            ///}
            //if (!avctx->width || !avctx->height || avctx->width > w || avctx->height > h) {
            //    avctx->width  = w;
            //    avctx->height = h;
            //}
            //src_linesize[0] = av_image_get_linesize(avctx->pix_fmt, avctx->width, 0);
            //src_linesize[1] = av_image_get_linesize(avctx->pix_fmt, avctx->width, 1);
            //src_linesize[2] = av_image_get_linesize(avctx->pix_fmt, avctx->width, 2);
            //src_data[0] = deocde_buffer.data; //(uint8_t*)buffer->data();
            //src_data[1] = src_data[0] + src_linesize[0] * avctx->height ;
            //src_data[2] = src_data[1] + src_linesize[1] * -(-avctx->height >>pix_desc->log2_chroma_h);
            //av_image_copy(frame->vframe->data, frame->vframe->linesize,
            //             src_data, src_linesize,
            //              avctx->pix_fmt, avctx->width, avctx->height);
			pthread_mutex_lock(&s->time_map_mutex);
            if (s->ts_map->count(s->decodeFramePTS) > 0) {
                	frame->vframe->pts = (*s->ts_map)[s->decodeFramePTS].pts;
                	frame->vframe->reordered_opaque = (*s->ts_map)[s->decodeFramePTS].reordered_opaque;
					frame->vframe->display_picture_number = (*s->ts_map)[s->decodeFramePTS].serial;
					frame->vframe->pkt_pos = (*s->ts_map)[s->decodeFramePTS].pos;
					frame->vframe->pkt_duration = (*s->ts_map)[s->decodeFramePTS].duration;
                	s->ts_map->erase(s->decodeFramePTS);
            }
			pthread_mutex_unlock(&s->time_map_mutex);
      
		} else if (ret == INFO_OUTPUT_FORMAT_CHANGED) {
			//av_free(frame);
			continue;
		} else if(ret == INFO_OUTPUT_BUFFERS_CHANGED ){
			//av_free(frame);
			continue;
		} else if(ret == INFO_TRY_AGAIN_LATER){
			//av_free(frame);
			continue;
		} else {
			///s->error = 1;
			av_log(avctx, AV_LOG_DEBUG,"%s: mediacodec_decode flag end of stream, set decode_done", __FUNCTION__);
			decode_done = 1;
			frame = (Frame*)av_mallocz(sizeof(Frame));
        	if (!frame) {
            	frame         = s->end_frame;
            	frame->status = AVERROR(ENOMEM);
            	s->end_frame  = NULL;
        	}else{
				frame->status = ret;
			}
		}
push_frame:
        while (true) {
            pthread_mutex_lock(&s->out_mutex);
            if (s->out_queue->size() >= OUTPUT_QUEUE_LEN) {
                pthread_mutex_unlock(&s->out_mutex);
                usleep(10000);
				//av_log(avctx, AV_LOG_ERROR,"%s: s->out_queue->size() >= %d", __FUNCTION__,OUTPUT_QUEUE_LEN);
				if(s->surface_cmd!=0){
					break;
				}
				if(s->stop_decode){
					break;
				}
                continue;
            }
            break;
        }
		//av_log(avctx, AV_LOG_ERROR,"%s: frame->status=%d", __FUNCTION__,frame->status );
		s->out_queue->push_back(frame);
        pthread_mutex_unlock(&s->out_mutex);
    } while (!decode_done && !s->stop_decode);

    s->thread_exited = true;

	if (isAttached) {
		if (s->jvm->DetachCurrentThread() < 0) {
	  		av_log(avctx, AV_LOG_ERROR,"%s: Could not detach thread from JVM", __FUNCTION__);
		}
	}
	
	av_log(avctx, AV_LOG_DEBUG,"%s: decode thread out", __FUNCTION__);

    return 0;
}


static void simple_bitstream_filter(uint8_t* data, int len)
{
    int i =0, tmp_len = 0;
    uint8_t* p = data;
    if(!data || len <=4){
    	return;
    }
    do{
        if(0==p[i] ){
            tmp_len =  (p[i+1] <<16) + (p[i+2]<<8) + p[i+3];
			p[i+1] = 0;
            p[i+2] = 0;
            p[i+3] = 1;
            i = i + 4 + tmp_len;
        }else{
        	break;
        }
    }while(i>0 && i<len);
}


static int Stagefright_decode_frame(AVCodecContext *avctx, void *data,
                                    int *got_frame, AVPacket *avpkt)
{
	av_log(NULL, AV_LOG_INFO, "%s into\n", __FUNCTION__);

    StagefrightContext *s = (StagefrightContext*)avctx->priv_data;
    Frame *frame;
    int status;
    int orig_size = 0; // = avpkt->size;
    //AVPacket pkt; //= *avpkt;
    AVFrame *ret_frame;
	//int input_queue_full = 0;

	if(!s->start_decode){
		av_log(NULL, AV_LOG_ERROR, "%s not recv start decoder cmd yet\n", __FUNCTION__);
		return orig_size;
	}
		
	if(s->state != decoder_state_start){
		av_log(NULL, AV_LOG_ERROR, "%s decoder not start yet\n", __FUNCTION__);
		return orig_size;
	}

	///if(s->error!=0){
	///	av_log(NULL, AV_LOG_ERROR, "%s decoder error happen\n", __FUNCTION__);
	///	return -1;
	///}
	
	if(avpkt != NULL){ ///input
	    orig_size = avpkt->size;
   		/////pkt = *avpkt;
		
		if (!s->thread_started) {
	        pthread_create(&s->decode_thread_id, NULL, &decode_thread, avctx);
	        s->thread_started = true;
	    }

		//av_log(NULL, AV_LOG_ERROR, "%s: (orig)avpkt->size=%d,avpkt->pts=%lld,avpkt->dts=%lld,avpkt->pos=%lld,avpkt->duration=%d,avctx->compression_level=%d\n",
		//	__FUNCTION__,avpkt->size,avpkt->pts,avpkt->dts,avpkt->pos,avpkt->duration,avctx->compression_level);
        
		
		///h.264 NAL-> annexb ,MP4 need, ts not need
		if(s->mp4_h264){
		    if (avpkt && avpkt->data) {
		        //av_bitstream_filter_filter(s->bsfc, avctx, NULL, &pkt.data, &pkt.size,
		        //                           avpkt->data, avpkt->size, avpkt->flags & AV_PKT_FLAG_KEY);
				simple_bitstream_filter(avpkt->data, avpkt->size);
				//avpkt = &pkt;
		    }
		}
		if(!s->dummy_buf) {
	        s->dummy_buf = (uint8_t*)av_malloc(avpkt->size);
	        if (!s->dummy_buf)
	            return AVERROR(ENOMEM);
	        s->dummy_bufsize = avpkt->size;
	        memcpy(s->dummy_buf, avpkt->data, avpkt->size);
    	}

		if (avpkt->data) {
			int wait_cnt = 0;
			while (true) {
				pthread_mutex_lock(&s->in_mutex);
	    		if (s->in_queue->size() >= INPUT_QUEUE_LEN) {
	            	pthread_mutex_unlock(&s->in_mutex);
					av_log(NULL, AV_LOG_WARNING, "%s in_queue full, s->in_queue->size() >= %d\n", __FUNCTION__,INPUT_QUEUE_LEN);
					usleep(10000);
					wait_cnt++;
					if(wait_cnt<4){
						continue;
					}else{
						av_log(NULL, AV_LOG_WARNING, "%s in_queue full, just return AVERROR_BUFFER_TOO_SMALL\n", __FUNCTION__);
						return AVERROR_BUFFER_TOO_SMALL;
					}
	        	}
	        	pthread_mutex_unlock(&s->in_mutex);
				break;
			}
				
			frame = (Frame*)av_mallocz(sizeof(Frame));
	        frame->status  = OK;
	        frame->size    = avpkt->size;
	        frame->key     = avpkt->flags & AV_PKT_FLAG_KEY ? 1 : 0;
			///AVPacket encode data to frame->buffer
	        frame->buffer  = (uint8_t*)av_malloc(avpkt->size);
	        if (!frame->buffer) {
	            av_freep(&frame);
	            return AVERROR(ENOMEM);
	        }
	        uint8_t *ptr = avpkt->data;
	        // The OMX.SEC decoder fails without this.
	        //if (avpkt->size == orig_size + avctx->extradata_size) {
			//	av_log(NULL, AV_LOG_ERROR, "%s avpkt->size=%d,orig_size=%d,avctx->extradata_size=%d", __FUNCTION__,avpkt->size,orig_size,avctx->extradata_size);
	        //    ptr += avctx->extradata_size;
	        //    frame->size = orig_size;
	        //}
	        memcpy(frame->buffer, ptr, orig_size);
			
	        ///if (avpkt == &pkt){
			///	av_log(NULL, AV_LOG_INFO, "%s free avpkt->data begin\n", __FUNCTION__);
	        ///    av_free(avpkt->data);
			///	av_log(NULL, AV_LOG_INFO, "%s free avpkt->data end\n", __FUNCTION__);
			///}

			pthread_mutex_lock(&s->time_map_mutex);
	        frame->time = ++s->frame_index;
	        (*s->ts_map)[frame->time].pts = avpkt->pts;
	        (*s->ts_map)[frame->time].reordered_opaque = avctx->reordered_opaque;
			(*s->ts_map)[frame->time].serial = avctx->compression_level;
			(*s->ts_map)[frame->time].pos = avpkt->pos;
			(*s->ts_map)[frame->time].duration = avpkt->duration;
			pthread_mutex_unlock(&s->time_map_mutex);

	    	pthread_mutex_lock(&s->in_mutex);
	        s->in_queue->push_back(frame);
	        pthread_cond_signal(&s->condition);
	        pthread_mutex_unlock(&s->in_mutex);
			
			//av_log(NULL, AV_LOG_ERROR, "%s: (filter)avpkt->size=%d,avpkt->pts=%lld,avpkt->dts=%lld,avpkt->pos=%lld,avpkt->duration=%d,frame->time=%lld,avctx->compression_level=%d\n",
			//	__FUNCTION__,avpkt->size,avpkt->pts,avpkt->dts,avpkt->pos,avpkt->duration,frame->time,avctx->compression_level);
        
    	} else {
			///empty avpkt will set source_done
			//av_log(NULL, AV_LOG_ERROR, "%s avpkt->data==NULL, reach file end\n", __FUNCTION__);
	        //frame->status  = ERROR_END_OF_STREAM;
	        //s->source_done = true;
    	}
		
		av_log(NULL, AV_LOG_INFO, "%s out\n", __FUNCTION__);
    	return orig_size;
	}else { //output 
		//av_log(NULL, AV_LOG_ERROR, "%s into, try to got a frame\n", __FUNCTION__);
		///get a decode out frame from out queue 
	    pthread_mutex_lock(&s->out_mutex);
	    if (s->out_queue->empty()){ 
			//av_log(NULL, AV_LOG_ERROR, "%s cannot got a decode frame, just return\n", __FUNCTION__);
			pthread_mutex_unlock(&s->out_mutex);
			return 0;
	    }

		if(s->pts_order == FRAME_PTS_ORDER_DISORDER && s->thread_exited!=true) { //if(isMTKDecoder(s)){
			if (s->out_queue->size() < OUTPUT_QUEUE_LEN){ 
				//av_log(NULL, AV_LOG_ERROR, "%s decode frame buffer not full yet(%d), just return\n", __FUNCTION__,s->out_queue->size());
				pthread_mutex_unlock(&s->out_mutex);
				return 0;
			}else{
				if( !have_diff_ts_frame(s->out_queue) ){
					s->out_queue->sort(compare_pts);
					//std::list<Frame*>::iterator it;
					//for (it=s->out_queue->begin(); it!=s->out_queue->end(); ++it){ 
					// 	if((*it)->vframe!=NULL){
					//		av_log(NULL, AV_LOG_ERROR, "%s pts->%lld\n", __FUNCTION__,(*it)->vframe->pts);
					//	}			   		
					//}
				}else{
					//av_log(NULL, AV_LOG_WARNING, "%s have_diff_ts_frame true not sort\n", __FUNCTION__);
				}
			}
		}
	   
	    frame = *s->out_queue->begin();

		if( s->pts_order==FRAME_PTS_ORDER_UNDETECT && s->thread_exited!=true ){
			s->pts_order = detect_frame_pts_order(s,frame);
		}
	    s->out_queue->erase(s->out_queue->begin());
	    pthread_mutex_unlock(&s->out_mutex);
		
	    ret_frame = frame->vframe;
	    status = frame->status;
		orig_size = frame->size;

		//av_log(NULL, AV_LOG_ERROR, "%s frame->status=%d,status=%d\n", __FUNCTION__,frame->status,status);
		
		//av_log(NULL, AV_LOG_INFO, "%s free frame begin\n", __FUNCTION__);
	    av_freep(&frame);
		//av_log(NULL, AV_LOG_INFO, "%s free frame end\n", __FUNCTION__);

	    //if (status == ERROR_END_OF_STREAM){
		//	av_log(NULL, AV_LOG_INFO, "%s status == ERROR_END_OF_STREAM\n", __FUNCTION__);
	    //    return 0;
	    //}
		
	    if (status != OK) {
			av_log(NULL, AV_LOG_ERROR, "%s: Decode failed: %d\n", __FUNCTION__, status);
	        if (status == AVERROR(ENOMEM)){
	            return status;
	        }
	        return -1;
	    }

		//av_log(NULL, AV_LOG_INFO, "%s free frame prev_frame begin\n", __FUNCTION__);
	    if (s->prev_frame){
	    	av_freep(&s->prev_frame); ///av_frame_free(&s->prev_frame);
	    }
		//av_log(NULL, AV_LOG_INFO, "%s free frame prev_frame end\n", __FUNCTION__);
	    s->prev_frame = ret_frame;

	    *got_frame = 1;
	    *(AVFrame*)data = *ret_frame;
		av_log(NULL, AV_LOG_INFO, "%s out\n", __FUNCTION__);
    	return orig_size;
	}
}


//encode data -> private ByteBuffer encodeFrame;
static int JNICALL readEncodeFrame(JNIEnv * env,jobject obj, jint nativeContext)
{
	Frame *frame;
	int ret = -1;
	av_log(NULL, AV_LOG_INFO, "%s: into\n",__FUNCTION__);

	
	StagefrightContext* stagefright_context = (StagefrightContext*)nativeContext;
	
	if(!stagefright_context){
		av_log(NULL, AV_LOG_ERROR, "%s: stagefright_context NULL\n",__FUNCTION__);
		return -1;
	}
	
	if (stagefright_context->thread_exited){
		av_log(NULL, AV_LOG_INFO, "%s: thread_exited\n",__FUNCTION__);
		return -1;
	}
	
		
	pthread_mutex_lock(&stagefright_context->in_mutex);
	
	if (stagefright_context->stop_decode){
		av_log(NULL, AV_LOG_ERROR, "%s: stop_decode\n",__FUNCTION__);
		pthread_mutex_unlock(&stagefright_context->in_mutex);
		return -1;
	}

	while (stagefright_context->in_queue->empty()){
		pthread_cond_wait(&stagefright_context->condition, &stagefright_context->in_mutex);
	}

	frame = *stagefright_context->in_queue->begin();
	ret = frame->status;
	if (ret == OK) {
		jobject encodeFrame = env->GetObjectField(stagefright_context->mediacodec_obj, stagefright_context->encodeFrameID);
		uint8_t *pencodeFrame = (uint8_t *)env->GetDirectBufferAddress(encodeFrame);
		memcpy(pencodeFrame, frame->buffer, frame->size);
		ret = frame->size;
		env->CallVoidMethod(stagefright_context->mediacodec_obj, stagefright_context->mediacodec_pts, frame->time);
	
		av_freep(&frame->buffer);
	}else{
		av_log(NULL, AV_LOG_INFO, "%s: cannot read a encode frame, ret=%d\n",__FUNCTION__,ret);
	}

	stagefright_context->in_queue->erase(stagefright_context->in_queue->begin());
	pthread_mutex_unlock(&stagefright_context->in_mutex);

	av_freep(&frame);

	av_log(NULL, AV_LOG_INFO, "%s: out\n",__FUNCTION__);
	
	return ret;

}

static jobject getSurface(JNIEnv * env,jobject obj, jint nativeContext)
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	StagefrightContext* stagefright_context = (StagefrightContext*)nativeContext;

	if(!stagefright_context){
		av_log(NULL, AV_LOG_ERROR, "%s: stagefright_context NULL\n",__FUNCTION__);
		return NULL;
	}
	if(stagefright_context->mediacodec_surface==NULL){
		av_log(NULL, AV_LOG_ERROR, "%s: stagefright_context->mediacodec_surface==NULL\n",__FUNCTION__);
		return NULL;
	}
	return stagefright_context->mediacodec_surface;
}

#define MAX_LOG2_MAX_FRAME_NUM    (12 + 4)
#define MIN_LOG2_MAX_FRAME_NUM    4
#define MAX_PICTURE_COUNT 			36

extern "C" {
	static const uint8_t default_scaling4[2][16] = {
		{  6, 13, 20, 28, 13, 20, 28, 32,
		  20, 28, 32, 37, 28, 32, 37, 42 },
		{ 10, 14, 20, 24, 14, 20, 24, 27,
		  20, 24, 27, 30, 24, 27, 30, 34 }
	};
	
	static const uint8_t default_scaling8[2][64] = {
		{  6, 10, 13, 16, 18, 23, 25, 27,
		  10, 11, 16, 18, 23, 25, 27, 29,
		  13, 16, 18, 23, 25, 27, 29, 31,
		  16, 18, 23, 25, 27, 29, 31, 33,
		  18, 23, 25, 27, 29, 31, 33, 36,
		  23, 25, 27, 29, 31, 33, 36, 38,
		  25, 27, 29, 31, 33, 36, 38, 40,
		  27, 29, 31, 33, 36, 38, 40, 42 },
		{  9, 13, 15, 17, 19, 21, 22, 24,
		  13, 13, 17, 19, 21, 22, 24, 25,
		  15, 17, 19, 21, 22, 24, 25, 27,
		  17, 19, 21, 22, 24, 25, 27, 28,
		  19, 21, 22, 24, 25, 27, 28, 30,
		  21, 22, 24, 25, 27, 28, 30, 32,
		  22, 24, 25, 27, 28, 30, 32, 33,
		  24, 25, 27, 28, 30, 32, 33, 35 }
	};
	static const uint8_t zigzag_scan[16+1] = {
		0 + 0 * 4, 1 + 0 * 4, 0 + 1 * 4, 0 + 2 * 4,
		1 + 1 * 4, 2 + 0 * 4, 3 + 0 * 4, 2 + 1 * 4,
		1 + 2 * 4, 0 + 3 * 4, 1 + 3 * 4, 2 + 2 * 4,
		3 + 1 * 4, 3 + 2 * 4, 2 + 3 * 4, 3 + 3 * 4,
	};
	static const uint8_t ff_zigzag_direct[64] = {
		0,	 1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63
	};

static void decode_scaling_list(StagefrightContext *h, uint8_t *factors, int size,
                                const uint8_t *jvt_list,
                                const uint8_t *fallback_list)
{
    int i, last = 8, next = 8;
    const uint8_t *scan = size == 16 ? zigzag_scan : ff_zigzag_direct;
    if (!get_bits1(&h->gb)) /* matrix not written, we use the predicted one */
        memcpy(factors, fallback_list, size * sizeof(uint8_t));
    else
        for (i = 0; i < size; i++) {
            if (next)
                next = (last + get_se_golomb(&h->gb)) & 0xff;
            if (!i && !next) { /* matrix not written, we use the preset one */
                memcpy(factors, jvt_list, size * sizeof(uint8_t));
                break;
            }
            last = factors[scan[i]] = next ? next : last;
        }
}

static void decode_scaling_matrices(StagefrightContext* h, H264SPS *sps,
                                    void *pps, int is_sps,
                                    uint8_t(*scaling_matrix4)[16],
                                    uint8_t(*scaling_matrix8)[64])
{
    int fallback_sps = !is_sps && sps->scaling_matrix_present;
    const uint8_t *fallback[4] = {
        fallback_sps ? sps->scaling_matrix4[0] : default_scaling4[0],
        fallback_sps ? sps->scaling_matrix4[3] : default_scaling4[1],
        fallback_sps ? sps->scaling_matrix8[0] : default_scaling8[0],
        fallback_sps ? sps->scaling_matrix8[3] : default_scaling8[1]
    };
    if (get_bits1(&h->gb)) {
        sps->scaling_matrix_present |= is_sps;
        decode_scaling_list(h, scaling_matrix4[0], 16, default_scaling4[0], fallback[0]);        // Intra, Y
        decode_scaling_list(h, scaling_matrix4[1], 16, default_scaling4[0], scaling_matrix4[0]); // Intra, Cr
        decode_scaling_list(h, scaling_matrix4[2], 16, default_scaling4[0], scaling_matrix4[1]); // Intra, Cb
        decode_scaling_list(h, scaling_matrix4[3], 16, default_scaling4[1], fallback[1]);        // Inter, Y
        decode_scaling_list(h, scaling_matrix4[4], 16, default_scaling4[1], scaling_matrix4[3]); // Inter, Cr
        decode_scaling_list(h, scaling_matrix4[5], 16, default_scaling4[1], scaling_matrix4[4]); // Inter, Cb
        if (is_sps ) {
            decode_scaling_list(h, scaling_matrix8[0], 64, default_scaling8[0], fallback[2]); // Intra, Y
            decode_scaling_list(h, scaling_matrix8[3], 64, default_scaling8[1], fallback[3]); // Inter, Y
            if (sps->chroma_format_idc == 3) {
                decode_scaling_list(h, scaling_matrix8[1], 64, default_scaling8[0], scaling_matrix8[0]); // Intra, Cr
                decode_scaling_list(h, scaling_matrix8[4], 64, default_scaling8[1], scaling_matrix8[3]); // Inter, Cr
                decode_scaling_list(h, scaling_matrix8[2], 64, default_scaling8[0], scaling_matrix8[1]); // Intra, Cb
                decode_scaling_list(h, scaling_matrix8[5], 64, default_scaling8[1], scaling_matrix8[4]); // Inter, Cb
            }
        }
    }
}


static int parse_seq_parameter_set(StagefrightContext* h, H264SPS *sps)
{
    int profile_idc, level_idc, constraint_set_flags = 0;
    unsigned int sps_id;
    int i, log2_max_frame_num_minus4;

	//av_log(h->avctx, AV_LOG_ERROR, "%s parse into\n", __FUNCTION__);

    profile_idc           = get_bits(&h->gb, 8);
    constraint_set_flags |= get_bits1(&h->gb) << 0;   // constraint_set0_flag
    constraint_set_flags |= get_bits1(&h->gb) << 1;   // constraint_set1_flag
    constraint_set_flags |= get_bits1(&h->gb) << 2;   // constraint_set2_flag
    constraint_set_flags |= get_bits1(&h->gb) << 3;   // constraint_set3_flag
    constraint_set_flags |= get_bits1(&h->gb) << 4;   // constraint_set4_flag
    constraint_set_flags |= get_bits1(&h->gb) << 5;   // constraint_set5_flag
    
    get_bits(&h->gb, 2); // reserved
    level_idc = get_bits(&h->gb, 8);
    sps_id    = get_ue_golomb_31(&h->gb);

    if (sps_id >= 32) {
        av_log(h->avctx, AV_LOG_ERROR, "sps_id (%d) out of range\n", sps_id);
        return AVERROR_INVALIDDATA;
    }
   

    sps->time_offset_length   = 24;
    sps->profile_idc          = profile_idc;
    sps->constraint_set_flags = constraint_set_flags;
    sps->level_idc            = level_idc;
    sps->full_range           = -1;

    memset(sps->scaling_matrix4, 16, sizeof(sps->scaling_matrix4));
    memset(sps->scaling_matrix8, 16, sizeof(sps->scaling_matrix8));
    sps->scaling_matrix_present = 0;
    sps->colorspace = (AVColorSpace)2; //AVCOL_SPC_UNSPECIFIED

	//av_log(h->avctx, AV_LOG_ERROR, "profile_idc(%d) level_idc(%d)\n", profile_idc,level_idc);

    if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244 ||
        sps->profile_idc ==  44 || sps->profile_idc ==  83 ||
        sps->profile_idc ==  86 || sps->profile_idc == 118 ||
        sps->profile_idc == 128 || sps->profile_idc == 144) {
        sps->chroma_format_idc = get_ue_golomb_31(&h->gb);
        if (sps->chroma_format_idc > 3U) {
            av_log(h->avctx, AV_LOG_ERROR,
                   "chroma_format_idc %d is illegal\n",
                   sps->chroma_format_idc);
            goto fail;
        } else if (sps->chroma_format_idc == 3) {
            sps->residual_color_transform_flag = get_bits1(&h->gb);
            if (sps->residual_color_transform_flag) {
                av_log(h->avctx, AV_LOG_ERROR, "separate color planes are not supported\n");
                goto fail;
            }
        }
        sps->bit_depth_luma   = get_ue_golomb(&h->gb) + 8;
        sps->bit_depth_chroma = get_ue_golomb(&h->gb) + 8;
        if (sps->bit_depth_luma > 14U || sps->bit_depth_chroma > 14U || sps->bit_depth_luma != sps->bit_depth_chroma) {
            av_log(h->avctx, AV_LOG_ERROR, "illegal bit depth value (%d, %d)\n",
                   sps->bit_depth_luma, sps->bit_depth_chroma);
            goto fail;
        }
        sps->transform_bypass = get_bits1(&h->gb);
        decode_scaling_matrices(h, sps, NULL, 1,
                                sps->scaling_matrix4, sps->scaling_matrix8);
    } else {
        sps->chroma_format_idc = 1;
        sps->bit_depth_luma    = 8;
        sps->bit_depth_chroma  = 8;
    }


    log2_max_frame_num_minus4 = get_ue_golomb(&h->gb);
    if (log2_max_frame_num_minus4 < MIN_LOG2_MAX_FRAME_NUM - 4 ||
        log2_max_frame_num_minus4 > MAX_LOG2_MAX_FRAME_NUM - 4) {
        av_log(h->avctx, AV_LOG_ERROR,
               "log2_max_frame_num_minus4 out of range (0-12): %d\n",
               log2_max_frame_num_minus4);
        goto fail;
    }
    sps->log2_max_frame_num = log2_max_frame_num_minus4 + 4;

    sps->poc_type = get_ue_golomb_31(&h->gb);

    if (sps->poc_type == 0) { // FIXME #define
        unsigned t = get_ue_golomb(&h->gb);
        if (t>12) {
            av_log(h->avctx, AV_LOG_ERROR, "log2_max_poc_lsb (%d) is out of range\n", t);
            goto fail;
        }
        sps->log2_max_poc_lsb = t + 4;
    } else if (sps->poc_type == 1) { // FIXME #define
        sps->delta_pic_order_always_zero_flag = get_bits1(&h->gb);
        sps->offset_for_non_ref_pic           = get_se_golomb(&h->gb);
        sps->offset_for_top_to_bottom_field   = get_se_golomb(&h->gb);
        sps->poc_cycle_length                 = get_ue_golomb(&h->gb);

        if ((unsigned)sps->poc_cycle_length >=
            FF_ARRAY_ELEMS(sps->offset_for_ref_frame)) {
            av_log(h->avctx, AV_LOG_ERROR,
                   "poc_cycle_length overflow %u\n", sps->poc_cycle_length);
            goto fail;
        }

        for (i = 0; i < sps->poc_cycle_length; i++)
            sps->offset_for_ref_frame[i] = get_se_golomb(&h->gb);
    } else if (sps->poc_type != 2) {
        av_log(h->avctx, AV_LOG_ERROR, "illegal POC type %d\n", sps->poc_type);
        goto fail;
    }

    sps->ref_frame_count = get_ue_golomb_31(&h->gb);
    if (h->avctx->codec_tag == MKTAG('S', 'M', 'V', '2'))
        sps->ref_frame_count = FFMAX(2, sps->ref_frame_count);
    if (sps->ref_frame_count > MAX_PICTURE_COUNT - 2 ||
        sps->ref_frame_count > 16U) {
        av_log(h->avctx, AV_LOG_ERROR, "too many reference frames\n");
        goto fail;
    }
    sps->gaps_in_frame_num_allowed_flag = get_bits1(&h->gb);
    sps->mb_width                       = get_ue_golomb(&h->gb) + 1;
    sps->mb_height                      = get_ue_golomb(&h->gb) + 1;
    if ((unsigned)sps->mb_width  >= INT_MAX / 16 ||
        (unsigned)sps->mb_height >= INT_MAX / 16 ||
        av_image_check_size(16 * sps->mb_width,
                            16 * sps->mb_height, 0, h->avctx)) {
        av_log(h->avctx, AV_LOG_ERROR, "mb_width/height overflow\n");
        goto fail;
    }

    sps->frame_mbs_only_flag = get_bits1(&h->gb);
    if (!sps->frame_mbs_only_flag)
        sps->mb_aff = get_bits1(&h->gb);
    else
        sps->mb_aff = 0;

    sps->direct_8x8_inference_flag = get_bits1(&h->gb);

//#ifndef ALLOW_INTERLACE
    if (sps->mb_aff)
        av_log(h->avctx, AV_LOG_ERROR,
               "MBAFF support not included; enable it at compile-time.\n");
//#endif
    sps->crop = get_bits1(&h->gb);
    if (sps->crop) {
        int crop_left   = get_ue_golomb(&h->gb);
        int crop_right  = get_ue_golomb(&h->gb);
        int crop_top    = get_ue_golomb(&h->gb);
        int crop_bottom = get_ue_golomb(&h->gb);
        int width  = 16 * sps->mb_width;
        int height = 16 * sps->mb_height * (2 - sps->frame_mbs_only_flag);

        if (h->avctx->flags2 & CODEC_FLAG2_IGNORE_CROP) {
            av_log(h->avctx, AV_LOG_DEBUG, "discarding sps cropping, original "
                                           "values are l:%u r:%u t:%u b:%u\n",
                   crop_left, crop_right, crop_top, crop_bottom);

            sps->crop_left   =
            sps->crop_right  =
            sps->crop_top    =
            sps->crop_bottom = 0;
        } else {
            int vsub   = (sps->chroma_format_idc == 1) ? 1 : 0;
            int hsub   = (sps->chroma_format_idc == 1 ||
                          sps->chroma_format_idc == 2) ? 1 : 0;
            int step_x = 1 << hsub;
            int step_y = (2 - sps->frame_mbs_only_flag) << vsub;

            if (crop_left & (0x1F >> (sps->bit_depth_luma > 8)) &&
                !(h->avctx->flags & CODEC_FLAG_UNALIGNED)) {
                crop_left &= ~(0x1F >> (sps->bit_depth_luma > 8));
                av_log(h->avctx, AV_LOG_WARNING,
                       "Reducing left cropping to %d "
                       "chroma samples to preserve alignment.\n",
                       crop_left);
            }

            if (crop_left  > (unsigned)INT_MAX / 4 / step_x ||
                crop_right > (unsigned)INT_MAX / 4 / step_x ||
                crop_top   > (unsigned)INT_MAX / 4 / step_y ||
                crop_bottom> (unsigned)INT_MAX / 4 / step_y ||
                (crop_left + crop_right ) * step_x >= width ||
                (crop_top  + crop_bottom) * step_y >= height
            ) {
                av_log(h->avctx, AV_LOG_ERROR, "crop values invalid %d %d %d %d / %d %d\n", crop_left, crop_right, crop_top, crop_bottom, width, height);
                goto fail;
            }

            sps->crop_left   = crop_left   * step_x;
            sps->crop_right  = crop_right  * step_x;
            sps->crop_top    = crop_top    * step_y;
            sps->crop_bottom = crop_bottom * step_y;
        }
    } else {
        sps->crop_left   =
        sps->crop_right  =
        sps->crop_top    =
        sps->crop_bottom =
        sps->crop        = 0;
    }

    sps->vui_parameters_present_flag = get_bits1(&h->gb);
    //if (sps->vui_parameters_present_flag)
    //    if (decode_vui_parameters(h, sps) < 0)
    //        goto fail;

    if (!sps->sar.den)
        sps->sar.den = 1;


    sps->new_ = 1;

    //av_free(h->sps_buffers[sps_id]);
    //h->sps_buffers[sps_id] = sps;
    //av_log(h->avctx, AV_LOG_ERROR, "%s parse OK,sps->mb_width=%d,sps->mb_height=%d\n", __FUNCTION__,sps->mb_width,sps->mb_height);

    return 0;

fail:
    //av_free(sps);
    av_log(h->avctx, AV_LOG_ERROR, "%s parse fail\n", __FUNCTION__);
    return -1;
}

}

static int parse_csd(StagefrightContext *s,uint8_t* extradata, int extradata_size,uint8_t **csd0,int *csd0_len, uint8_t **csd1, int* csd1_len)
{
	if(!extradata){
		av_log(NULL, AV_LOG_ERROR, "%s extradata NULL\n", __FUNCTION__);
		return -1;
	}
	if(extradata_size<=8){
		av_log(NULL, AV_LOG_ERROR, "%s extradata_size error\n", __FUNCTION__);
		return -1;
	}
	//for(int i=0;i<extradata_size;i++){
	//	av_log(NULL, AV_LOG_ERROR, "%s extradata[%d]=%d\n", __FUNCTION__,i,(int8_t)extradata[i]);	
	//}
	if(1==extradata[0]){
		//av_log(NULL, AV_LOG_ERROR, "%s extradata[0]->1\n", __FUNCTION__);

		s->mp4_h264 = 1;
		unsigned int len = (extradata[6]<<8) + extradata[7];
		//av_log(NULL, AV_LOG_ERROR, "%s len=%d\n", __FUNCTION__,len);
		if(len>extradata_size-8){
			av_log(NULL, AV_LOG_ERROR, "%s csd0 len error, len=%d\n", __FUNCTION__,len);
			return -1;
		}
		uint8_t* pcsd = *csd0 = (uint8_t*)malloc(len+4);
		if(!pcsd){
			av_log(NULL, AV_LOG_ERROR, "%s outof memory\n", __FUNCTION__);
			return -1;
		}
		pcsd[0] = 0;pcsd[1] = 0;pcsd[2] = 0;pcsd[3] = 1;
		memcpy(pcsd+4,&extradata[8],len);
		*csd0_len = len+4;
		//av_log(NULL, AV_LOG_ERROR, "%s csd0 len=%d\n", __FUNCTION__,*csd0_len);
		//for(int j=0;j<*csd0_len;j++){
		//	av_log(NULL, AV_LOG_ERROR, "%s csd0[%d]=%d\n", __FUNCTION__,j,(int8_t)pcsd[j]);
		//}

		len = (extradata[8+len+1]<<8) + extradata[8+len+2];
		if(len>extradata_size-*csd0_len+4-8){
			av_log(NULL, AV_LOG_ERROR, "%s csd1 len error, len=%d\n", __FUNCTION__,len);
			return -1;
		}
		pcsd = *csd1 = (uint8_t*)malloc(len+4);
		if(!pcsd){
			av_log(NULL, AV_LOG_ERROR, "%s outof memory\n", __FUNCTION__);
			return -1;
		}
		pcsd[0] = 0;pcsd[1] = 0;pcsd[2] = 0;pcsd[3] = 1;
		memcpy(pcsd+4,&extradata[8+*csd0_len-4+3],len);
		*csd1_len = len+4;
		//av_log(NULL, AV_LOG_DEBUG, "%s csd1 len=%d\n", __FUNCTION__,*csd1_len);
		//for(int j=0;j<*csd1_len;j++){
		//	av_log(NULL, AV_LOG_ERROR, "%s csd1[%d]=%d\n", __FUNCTION__,j,(int8_t)pcsd[j]);
		//}
	}else{
		//av_log(NULL, AV_LOG_ERROR, "%s extradata[0]->!1\n", __FUNCTION__);

		s->mp4_h264 = 0;
		int start = 6, divd=0;
		if( extradata[start]==0 && extradata[start+1]==0 && extradata[start+2]==0 && extradata[start+3]==1){
			divd = 4;
		}else if( extradata[start]==0 && extradata[start+1]==0 && extradata[start+2]==1){
			divd = 3;
		}else{
			av_log(NULL, AV_LOG_ERROR, "%s divd unknow fotmat\n", __FUNCTION__);
			return -1;
		}
		//av_log(NULL, AV_LOG_ERROR, "%s divd=%d\n", __FUNCTION__,divd);
		int cnt = 0;
		start = start + divd;
		while(1){
			if(divd==4){
				if(extradata[start+cnt]==0 && extradata[start+cnt+1]==0&& extradata[start+cnt+2]==0&& extradata[start+cnt+3]==1){
					break;
				}
			}else{
				if(extradata[start+cnt]==0 && extradata[start+cnt+1]==0&& extradata[start+cnt+2]==1){
					break;
				}
			}
			cnt++;
			if(cnt>extradata_size-start){
				av_log(NULL, AV_LOG_ERROR, "%s csd0 unknow fotmat\n", __FUNCTION__);
				return -1;
			}
		}
		//av_log(NULL, AV_LOG_ERROR, "%s csd0 cnt=%d\n", __FUNCTION__,cnt);
		cnt = cnt;
		uint8_t* pcsd = *csd0 = (uint8_t*)malloc(cnt+4);
		if(!pcsd){
			av_log(NULL, AV_LOG_ERROR, "%s outof memory\n", __FUNCTION__);
			return -1;
		}
		pcsd[0] = 0;pcsd[1] = 0;pcsd[2] = 0;pcsd[3] = 1;
		memcpy(pcsd+4,&extradata[start],cnt);
		*csd0_len = cnt+4;
		//av_log(NULL, AV_LOG_ERROR, "%s csd0 len=%d\n", __FUNCTION__,*csd0_len);
		//for(int j=0;j<*csd0_len;j++){
		//	av_log(NULL, AV_LOG_ERROR, "%s csd0[%d]=%d\n", __FUNCTION__,j,(int8_t)pcsd[j]);
		//}
		H264SPS sps;
		init_get_bits(&s->gb, (uint8_t*)&pcsd[5], cnt*8);
		if( !parse_seq_parameter_set( s, &sps) ){
			s->width = sps.mb_width*16;
			s->height = sps.mb_height*16;
		}
		//csd1
		start = start + cnt + divd;
		if(start>extradata_size){
			av_log(NULL, AV_LOG_ERROR, "%s csd1 unknow fotmat\n", __FUNCTION__);
			return -1;
		}
		//av_log(NULL, AV_LOG_ERROR, "%s csd1 start=%d\n", __FUNCTION__,start);
		cnt = extradata_size - start;
		//av_log(NULL, AV_LOG_ERROR, "%s csd1 cnt=%d\n", __FUNCTION__,cnt);
		pcsd = *csd1 = (uint8_t*)malloc(cnt+4);
		if(!pcsd){
			av_log(NULL, AV_LOG_ERROR, "%s outof memory\n", __FUNCTION__);
			return -1;
		}
		pcsd[0] = 0;pcsd[1] = 0;pcsd[2] = 0;pcsd[3] = 1;
		memcpy(pcsd+4,&extradata[start],cnt);
		*csd1_len = cnt+4;
		//av_log(NULL, AV_LOG_ERROR, "%s csd1 len=%d\n", __FUNCTION__,*csd1_len);
		//for(int j=0;j<*csd1_len;j++){
		//	av_log(NULL, AV_LOG_ERROR, "%s csd1[%d]=%d\n", __FUNCTION__,j,(int8_t)pcsd[j]);
		//}
	}
	return 0;
}
static av_cold int Stagefright_init(AVCodecContext *avctx)
{
    StagefrightContext *s = (StagefrightContext*)avctx->priv_data; 
    int32_t colorFormat = 0;
    int ret;
	uint8_t *csd0=NULL; 
	uint8_t* csd1=NULL;
	int csd0_len=0, csd1_len=0;
	

	av_log(avctx, AV_LOG_DEBUG, "%s into\n", __FUNCTION__);
	 
    if (!avctx->extradata || !avctx->extradata_size ){
		av_log(avctx, AV_LOG_ERROR, "%s: param error,avctx->extradata NULL,avctx->extradata_size=%d\n",__FUNCTION__,avctx->extradata_size);
        return -1;
    }

    s->avctx = avctx;
    s->bsfc  = av_bitstream_filter_init("h264_mp4toannexb");
    if (!s->bsfc) {
        av_log(avctx, AV_LOG_ERROR, "Cannot open the h264_mp4toannexb BSF!\n");
        return -1;
    }

    s->orig_extradata_size = avctx->extradata_size;
    s->orig_extradata = (uint8_t*) av_mallocz(avctx->extradata_size +
                                              FF_INPUT_BUFFER_PADDING_SIZE);
    if (!s->orig_extradata) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    memcpy(s->orig_extradata, avctx->extradata, avctx->extradata_size);

	
	//av_log(avctx, AV_LOG_ERROR, "%s: parse csd\n",__FUNCTION__);
	ret = parse_csd(s,avctx->extradata, avctx->extradata_size,&csd0, &csd0_len,&csd1,&csd1_len);
	if(-1==ret){
		av_log(avctx, AV_LOG_ERROR,  "%s: parse_csd fail", __FUNCTION__);
		ret = -1;
    	goto fail;
	}
	s->csd0 = csd0;
	s->csd0_len = csd0_len;
	s->csd1 = csd1;
	s->csd1_len = csd1_len;
	
	if(avctx->width==0 || avctx->height==0){
		avctx->width = s->width;
		avctx->height = s->height;
		av_log(avctx, AV_LOG_ERROR, "%s: wrong width and hieght,set to real width and height,avctx->width=%d,avctx->height=%d\n",__FUNCTION__,avctx->width,avctx->height);
	}else{
		s->width = avctx->width;
		s->height = avctx->height;
	}
	
    s->in_queue  = new std::list<Frame*>;
    s->out_queue = new std::list<Frame*>;
    s->ts_map    = new std::map<int64_t, TimeStamp>;

    s->end_frame = (Frame*)av_mallocz(sizeof(Frame));
    if ( !s->in_queue || !s->out_queue  || !s->ts_map || !s->end_frame) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }


    //outFormat = (*s->decoder)->getFormat();
    //outFormat->findInt32(kKeyColorFormat, &colorFormat);
    //if (colorFormat == OMX_QCOM_COLOR_FormatYVU420SemiPlanar ||
    //    colorFormat == OMX_COLOR_FormatYUV420SemiPlanar)
    //    avctx->pix_fmt = AV_PIX_FMT_NV21;
    //else if (colorFormat == OMX_COLOR_FormatYCbYCr)
    //    avctx->pix_fmt = AV_PIX_FMT_YUYV422;
    //else if (colorFormat == OMX_COLOR_FormatCbYCrY)
    //    avctx->pix_fmt = AV_PIX_FMT_UYVY422;
    //else
    avctx->pix_fmt = AV_PIX_FMT_YUV420P;

  
    //if (s->decoder_component)
    //    s->decoder_component = av_strdup(s->decoder_component);
    s->decoder_component = NULL;

    pthread_mutex_init(&s->in_mutex, NULL);
    pthread_mutex_init(&s->out_mutex, NULL);
    pthread_cond_init(&s->condition, NULL);
	pthread_mutex_init(&s->time_map_mutex, NULL);

	//s->videoframe_pts_base = -1; //(the frame with min pts)
	//s->videoframe_pos_base = -1; //(first frame's pos)
	//s->videoframe_serial_last = -1;
	//s->videoframe_pos_last = -1; // (new pts)
	//s->videoframe_pts_last = -1; // (new pos)
	//s->videoframe_base_index = -1;

	s->pts_order = FRAME_PTS_ORDER_UNDETECT;
	s->last_frame_pts = -1;
	
	///s->error = 0;

	av_log(avctx, AV_LOG_DEBUG, "%s init success\n", __FUNCTION__);
    return 0;

fail:
    av_bitstream_filter_close(s->bsfc);
    av_freep(&s->orig_extradata);
    av_freep(&s->end_frame);
    delete s->in_queue;
    delete s->out_queue;
    delete s->ts_map;

	///s->error = 1;
   
	av_log(avctx, AV_LOG_ERROR, "%s init fail\n", __FUNCTION__);
    return ret;
}

static int mediacodec_init(StagefrightContext* s)
{
	//av_log(NULL, AV_LOG_ERROR, "%s into\n", __FUNCTION__);

	bool isAttached = false;
	JNIEnv* env = NULL;
	int ret = -1;
	jobject csd0Array = 0;
	uint8_t *pcsd0Array =0;
	jobject csd1Array = 0;
	uint8_t *pcsd1Array = 0;
	jsize csdArraySize;
	
	//void *jvm_, *mclass_, *msurface_;
	//avcodec_get_jvm(&jvm_, &mclass_);
	//avcodec_get_surface(&msurface_);
	
	if(s->jvm==NULL || s->mediacodec_class==NULL || s->mediacodec_surface==NULL){
		av_log(NULL, AV_LOG_ERROR, "%s param NULL, s->jvm=%0x,s->mediacodec_class=%0x, s->mediacodec_surface=%0x\n", 
			__FUNCTION__,s->jvm,s->mediacodec_class,s->mediacodec_surface);
		return -1;
	}
	
	s->surface_cmd = 0;
	//s->flush_cmd = 0;
		
	JNINativeMethod nativeFunctions[3] = {
		{	"readEncodeFrame",
			"(I)I",
			(void*) &readEncodeFrame, 
		},
		{	"writeDecodeFrame",
			"(IJ)I",
			(void*) &writeDecodeFrame 
		},
		{	"getSurface",
			"(I)Landroid/view/Surface;",
			(void*) &getSurface 	
		},
	};

	if (s->jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		ret = s->jvm->AttachCurrentThread(&env, NULL);
		if ((ret < 0) || !env) {
		  av_log(NULL, AV_LOG_ERROR,"%s: funck! Could not attach thread to JVM (%d, %p)", __FUNCTION__, ret, env);
		  return -1;
		}
		isAttached = true;
	}

	
	///1 register native function
  	if (env->RegisterNatives(s->mediacodec_class, nativeFunctions, 3) != 0){
    	av_log(NULL, AV_LOG_ERROR, "%s: Failed to register native functions", __FUNCTION__);
    	ret = -1;
    	return ret;
  	}
  	///2 get method and fild id
  	s->mediacodec_create = env->GetStaticMethodID(s->mediacodec_class, "create", "()Lcom/sina/sinavideo/coreplayer/splayer/SMedia;");
	if (s->mediacodec_create == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get create ID", __FUNCTION__);
	  	ret = -1;
    	return ret;
	}
	s->mediacodec_start = env->GetMethodID(s->mediacodec_class, "start", "(III)I");
	if (s->mediacodec_start == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get start ID", __FUNCTION__);
	  	ret = -1;
    	return ret;
	}
	s->mediacodec_stop = env->GetMethodID(s->mediacodec_class, "stop", "(I)V");
	if (s->mediacodec_stop == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get stop ID", __FUNCTION__);
	  	ret = -1;
    	return ret;
	}
	s->mediacodec_decode = env->GetMethodID(s->mediacodec_class, "decode", "(I)I");
	if (s->mediacodec_decode == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get decode ID", __FUNCTION__);
	  	ret = -1;
    	return ret;
	}
	s->mediacodec_alloc = env->GetMethodID(s->mediacodec_class, "alloc", "(III)V");
	if (s->mediacodec_alloc == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get alloc ID", __FUNCTION__);
	  	ret = -1;
    	return ret;
	}
	s->mediacodec_pts = env->GetMethodID(s->mediacodec_class, "pts", "(J)V");
	if (s->mediacodec_pts == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get pts ID", __FUNCTION__);
	  	ret = -1;
    	return ret;
	}
	//
	s->mediacodec_registerContext = env->GetMethodID(s->mediacodec_class, "registerContext", "(I)V");
	if (s->mediacodec_registerContext == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get registerContext() ID", __FUNCTION__);
	  	ret = -1;
    	return ret;
	}
	//
	//s->mediacodec_flush = env->GetMethodID(s->mediacodec_class, "flush", "(II)V");
	//if (s->mediacodec_flush == NULL) {
	//  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get flush ID", __FUNCTION__);
	//  	ret = -1;
    //	return ret;
	//}
	s->csd0ArrayID = env->GetFieldID(s->mediacodec_class, "csd0Array", "Ljava/nio/ByteBuffer;");
	if (s->csd0ArrayID == NULL) {
	  	av_log(NULL, AV_LOG_ERROR,  "%s: could not get csd0Array ID", __FUNCTION__);
	 	ret = -1;
    	return ret;
	}	
	s->csd1ArrayID = env->GetFieldID(s->mediacodec_class, "csd1Array", "Ljava/nio/ByteBuffer;");
	if (s->csd1ArrayID == NULL) {
		av_log(NULL, AV_LOG_ERROR,  "%s: could not get csd1Array ID", __FUNCTION__);
		ret = -1;
		return ret;
	}
	s->encodeFrameID = env->GetFieldID(s->mediacodec_class, "encodeFrame", "Ljava/nio/ByteBuffer;");
	if (s->encodeFrameID == NULL) {
		av_log(NULL, AV_LOG_ERROR,  "%s: could not get encodeFrame ID", __FUNCTION__);
		ret = -1;
    	return ret;
	}
	s->decodeFrameID = env->GetFieldID(s->mediacodec_class, "decodeFrame", "Ljava/nio/ByteBuffer;");
	if (s->decodeFrameID == NULL) {
		av_log(NULL, AV_LOG_ERROR,  "%s: could not get decodeFrame ID", __FUNCTION__);
		ret = -1;
    	return ret;
	}

	//s->mCodecNameID = env->GetFieldID(s->mediacodec_class, "mCodecName", "Ljava/lang/String;");
	//if (s->mCodecNameID == NULL) {
	//	av_log(NULL, AV_LOG_ERROR,  "%s: could not get mCodecName ID", __FUNCTION__);
	//	ret = -1;
    //	return ret;
	//}

	///3 call mediacodec create 
	s->mediacodec_obj = env->CallStaticObjectMethod(s->mediacodec_class,s->mediacodec_create);
	if(NULL==s->mediacodec_obj){
		av_log(NULL, AV_LOG_ERROR,  "%s: call create fail", __FUNCTION__);
		ret = -1;
    	return ret;
	}
	s->mediacodec_obj = env->NewGlobalRef(s->mediacodec_obj);
	int ncontxt = (int)((void*)s);
	//av_log(NULL, AV_LOG_ERROR,  "%s: ncontxt=%ld", __FUNCTION__,ncontxt);
	env->CallVoidMethod(s->mediacodec_obj, s->mediacodec_registerContext,ncontxt);

	///alloc buffer for csd0 csd1
	env->CallVoidMethod(s->mediacodec_obj, s->mediacodec_alloc, ncontxt, 1, s->csd0_len);
	env->CallVoidMethod(s->mediacodec_obj, s->mediacodec_alloc, ncontxt, 0, s->csd1_len);
	///for csd0
	csd0Array = env->GetObjectField(s->mediacodec_obj, s->csd0ArrayID);
	if(!csd0Array){
		av_log(NULL, AV_LOG_ERROR,  "%s: cannot get csd0Array", __FUNCTION__);
		ret = -1;
    	return ret;
	}
	csd0Array = env->NewGlobalRef(csd0Array);
	if(!csd0Array){
		av_log(NULL, AV_LOG_ERROR,  "%s: NewGlobalRef csd0Array fail", __FUNCTION__);
		ret = -1;
    	return ret;
	}
	csdArraySize = env->GetDirectBufferCapacity(csd0Array);
	//av_log(NULL, AV_LOG_INFO, "%s: csd0ArraySize=%d\n",__FUNCTION__,csdArraySize);
	pcsd0Array = (uint8_t*)env->GetDirectBufferAddress(csd0Array);
	//av_log(NULL, AV_LOG_ERROR, "%s: xxxx\n",__FUNCTION__);
	if(!pcsd0Array){
		av_log(NULL, AV_LOG_ERROR,  "%s: pcsd0Array NULL\n", __FUNCTION__);
		ret = -1;
		return ret;
	}
	if(s->csd0_len>csdArraySize){
		s->csd0_len = csdArraySize;
	}
	//av_log(NULL, AV_LOG_ERROR, "%s: csd0->pcsd0Array\n",__FUNCTION__);
	memcpy(pcsd0Array,s->csd0,s->csd0_len);
	///for csd1
	csd1Array = env->GetObjectField(s->mediacodec_obj, s->csd1ArrayID);
	if(!csd1Array){
		av_log(NULL, AV_LOG_ERROR,  "%s: cannot get csd1Array", __FUNCTION__);
		ret = -1;
		return ret;
	}
	csd1Array = env->NewGlobalRef(csd1Array);
	if(!csd1Array){
		av_log(NULL, AV_LOG_ERROR,  "%s: NewGlobalRef csd1Array fail", __FUNCTION__);
		ret = -1;
		return ret;
	}
	csdArraySize = env->GetDirectBufferCapacity(csd1Array);
	//av_log(NULL, AV_LOG_ERROR, "%s: csd1ArraySize=%d\n",__FUNCTION__,csdArraySize);
	pcsd1Array = (uint8_t*)env->GetDirectBufferAddress(csd1Array);
	if(!pcsd1Array){
		av_log(NULL, AV_LOG_ERROR,  "%s: pcsd1Array NULL", __FUNCTION__);
		ret = -1;
		return ret;
	}
	if(s->csd1_len>csdArraySize){
		s->csd1_len = csdArraySize;
	}
	//av_log(NULL, AV_LOG_ERROR, "%s: csd1->pcsd1Array\n",__FUNCTION__);
	memcpy(pcsd1Array,s->csd1,s->csd1_len);
		

	///5 call start

	ret = env->CallIntMethod(s->mediacodec_obj, s->mediacodec_start,ncontxt,s->width,s->height);
	if(0==ret){
		s->state = decoder_state_start;
		/*
		///got codec name
		jobject codecName = env->GetObjectField(s->mediacodec_obj, s->mCodecNameID);
		if(codecName){
			const char *tmp = env->GetStringUTFChars(codecName, NULL);
	    	if (tmp != NULL) {
				s->decoder_component = malloc(strlen(tmp)+1);
				if(NULL!=s->decoder_component){
					memset(s->decoder_component,0,strlen(tmp)+1);
					strcpy(s->decoder_component,tmp);

					//if( strstr(s->decoder_component,"MTK")!=NULL || strstr(s->decoder_component,"mtk")!=NULL){
					//	s->dtype = DT_MTK_DECODER;
					//}else{
					//	s->dtype = DT_UNKNOW_DECODER;
					//}
					av_log(NULL, AV_LOG_ERROR,  "%s: s->decoder_component=%s,s->dtype=%d", __FUNCTION__,s->decoder_component,s->dtype);
				}
				env->ReleaseStringUTFChars(codecName, tmp);
			}
		}
		*/
	}else{
		av_log(NULL, AV_LOG_ERROR,  "%s: call start fail", __FUNCTION__);
	}

	// Detach this thread if it was attached
	if (isAttached) {
		if (s->jvm->DetachCurrentThread() < 0) {
		  	av_log(NULL, AV_LOG_ERROR,"%s: Could not detach thread from JVM", __FUNCTION__);
		}
	}
	
	//av_log(NULL, AV_LOG_ERROR, "%s out,ret=%d\n", __FUNCTION__,ret);

	return ret;

}

static int mediacodec_close(StagefrightContext* s)
{
	bool isAttached = false;
	int ret = -1;
	JNIEnv* env = NULL;
	av_log(NULL, AV_LOG_DEBUG,"%s: into", __FUNCTION__);

	
	if (s->jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		ret = s->jvm->AttachCurrentThread(&env, NULL);
		if ((ret < 0) || !env) {
		  av_log(NULL, AV_LOG_ERROR,"%s: Could not attach thread to JVM (%d, %p)", __FUNCTION__, ret, env);
		  return -1;
		}
		isAttached = true;
	}
	if(s->state==decoder_state_start){
		int ncontxt = (int)((void*)s);
		//av_log(NULL, AV_LOG_DEBUG,"%s: ncontxt=%ld", __FUNCTION__,ncontxt);
		env->CallVoidMethod(s->mediacodec_obj, s->mediacodec_stop,ncontxt);
	}
	env->DeleteGlobalRef(s->mediacodec_obj);

	s->state = decoder_state_release;
	///env->DeleteGlobalRef(mediacodec_class);
	if (isAttached) {
		if (s->jvm->DetachCurrentThread() < 0) {
		  	av_log(NULL, AV_LOG_ERROR,"%s: Could not detach thread from JVM", __FUNCTION__);
		}
	}
	av_log(NULL, AV_LOG_DEBUG,"%s: out", __FUNCTION__);
	return 0;

}
static av_cold int Stagefright_close(AVCodecContext *avctx)
{
	av_log(NULL, AV_LOG_DEBUG, "%s into\n", __FUNCTION__);

    StagefrightContext *s = (StagefrightContext*)avctx->priv_data;
    Frame *frame;
	int ret;

    if (s->thread_started) {
        if (!s->thread_exited) {
			av_log(NULL, AV_LOG_DEBUG, "%s set stop_decode\n", __FUNCTION__);
            s->stop_decode = 1;

            // Make sure decode_thread() doesn't get stuck
            av_log(NULL, AV_LOG_DEBUG, "%s emty out_queue begin\n", __FUNCTION__);
            pthread_mutex_lock(&s->out_mutex);
            while (!s->out_queue->empty()) {
                frame = *s->out_queue->begin();
                s->out_queue->erase(s->out_queue->begin());
                if (frame->vframe)
                    av_frame_free(&frame->vframe);
                av_freep(&frame);
            }
            pthread_mutex_unlock(&s->out_mutex);
			av_log(NULL, AV_LOG_DEBUG, "%s emty out_queue end\n", __FUNCTION__);

            // Feed a dummy frame prior to signalling EOF.
            // This is required to terminate the decoder(OMX.SEC)
            // when only one frame is read during stream info detection.
           
            if (s->dummy_buf && (frame = (Frame*)av_mallocz(sizeof(Frame)))) {
                frame->status = OK;
                frame->size   = s->dummy_bufsize;
                frame->key    = 1;
                frame->buffer = s->dummy_buf;
                pthread_mutex_lock(&s->in_mutex);
                s->in_queue->push_back(frame);
                pthread_cond_signal(&s->condition);
                pthread_mutex_unlock(&s->in_mutex);
                s->dummy_buf = NULL;
				av_log(NULL, AV_LOG_DEBUG, "%s push dummy frame in_queue\n", __FUNCTION__);
            }
		

            pthread_mutex_lock(&s->in_mutex);
            s->end_frame->status = ERROR_END_OF_STREAM;
            s->in_queue->push_back(s->end_frame);
            pthread_cond_signal(&s->condition);
            pthread_mutex_unlock(&s->in_mutex);
            s->end_frame = NULL;
			av_log(NULL, AV_LOG_DEBUG, "%s push end frame in_queue\n", __FUNCTION__);
        }

		av_log(NULL, AV_LOG_DEBUG, "%s wait decode_thread quit\n", __FUNCTION__);
        pthread_join(s->decode_thread_id, NULL);
		av_log(NULL, AV_LOG_DEBUG, "%s wait decode_thread quit end\n", __FUNCTION__);

        if (s->prev_frame){
             av_freep(&s->prev_frame); ///av_frame_free(&s->prev_frame);
        }

        s->thread_started = false;
    }

	av_log(NULL, AV_LOG_DEBUG, "%s empty in_queue last\n", __FUNCTION__);
    while (!s->in_queue->empty()) {
        frame = *s->in_queue->begin();
        s->in_queue->erase(s->in_queue->begin());
        if (frame->size)
            av_freep(&frame->buffer);
        av_freep(&frame);
    }
	
	av_log(NULL, AV_LOG_DEBUG, "%s empty out_queue last\n", __FUNCTION__);
    while (!s->out_queue->empty()) {
        frame = *s->out_queue->begin();
        s->out_queue->erase(s->out_queue->begin());
        if (frame->vframe)
            av_frame_free(&frame->vframe);
        av_freep(&frame);
    }

    av_log(avctx, AV_LOG_DEBUG,"%s: closing smedia begin", __FUNCTION__);
    mediacodec_close(s);

	av_log(avctx, AV_LOG_DEBUG,"%s: free rsources", __FUNCTION__);
    if (s->decoder_component){
        free(s->decoder_component);
    }
    av_freep(&s->dummy_buf);
    av_freep(&s->end_frame);

    // Reset the extradata back to the original mp4 format, so that
    // the next invocation (both when decoding and when called from
    // av_find_stream_info) get the original mp4 format extradata.
    av_freep(&avctx->extradata);
    avctx->extradata = s->orig_extradata;
    avctx->extradata_size = s->orig_extradata_size;

    delete s->in_queue;
    delete s->out_queue;
    delete s->ts_map;


    pthread_mutex_destroy(&s->in_mutex);
    pthread_mutex_destroy(&s->out_mutex);
    pthread_cond_destroy(&s->condition); 
	pthread_mutex_destroy(&s->time_map_mutex);
    av_bitstream_filter_close(s->bsfc);

	if(s->csd0){ 
		free(s->csd0);
		s->csd0 = NULL;
	}
	if(s->csd1){ 
		free(s->csd1);
		s->csd1 = NULL;
	}
	
	av_log(avctx, AV_LOG_DEBUG,"%s: out", __FUNCTION__);
    return 0;
}

static void Stagefright_flush(AVCodecContext *avctx)
{
	//av_log(avctx, AV_LOG_ERROR,"%s: into", __FUNCTION__);
	int ret ;
	StagefrightContext *s = (StagefrightContext*)avctx->priv_data; 

	if(avctx->opaque!=NULL){
		cmd_data* cmd = (cmd_data* )avctx->opaque;
		av_log(avctx, AV_LOG_DEBUG, "%s cmd->type=%d\n", __FUNCTION__,cmd->type);
		if( cmd->type == ANDROID_DECODER_START_CMD){
			ret = mediacodec_init(s);
			if( ret<0 ){
				av_log(avctx, AV_LOG_ERROR, "%s mediacodec_init fail\n", __FUNCTION__);
				avctx->opaque = (void*)-1;
				///s->error = 1;
			}else{
				s->start_decode = 1;
				avctx->opaque = (void*)0;
				///s->error = 0;
			}
		}else if( cmd->type == ANDROID_SURFACE_CREATE_CMD){
			/*
			if(s->state==decoder_state_start){
				av_log(avctx, AV_LOG_ERROR, "%s decoder already start\n", __FUNCTION__);
				return;
			}
			void *msurface_;
			avcodec_get_surface(&msurface_);
			s->mediacodec_surface = (jobject)msurface_;
			s->surface_cmd = ANDROID_SURFACE_CREATE_CMD;
			while(true){
				if(0 != s->surface_cmd){
					usleep(10000);
					av_log(avctx, AV_LOG_ERROR, "%s ANDROID_SURFACE_CREATE_CMD not handle yet\n", __FUNCTION__);
				}else{
					break;
				}
			}
			*/
		}else if( cmd->type  == ANDROID_SURFACE_DESTROY_CMD ){
			/*
			if(s->state==decoder_state_release || s->state==decoder_state_null){
				av_log(avctx, AV_LOG_ERROR, "%s decoder already destroy\n", __FUNCTION__);
				return;
			}
			void *msurface_;
			avcodec_get_surface(&msurface_);
			s->mediacodec_surface = (jobject)msurface_;
			s->surface_cmd = ANDROID_SURFACE_DESTROY_CMD;
			while(true){
				if(0 != s->surface_cmd){
					usleep(10000);
					av_log(avctx, AV_LOG_ERROR, "%s ANDROID_SURFACE_DESTROY_CMD not handle yet\n", __FUNCTION__);
				}else{
					break;
				}
			}*/
		}else if( cmd->type  == ANDROID_SET_JVM_CMD ){
			s->jvm = (JavaVM *)cmd->data;
			avctx->opaque = (void*)0;
		}else if( cmd->type  == ANDROID_SET_MCLASS_CMD ){
			s->mediacodec_class = (jclass)cmd->data;
			avctx->opaque = (void*)0;
		}else if( cmd->type  == ANDROID_SET_SURFACE_CMD ){
			s->mediacodec_surface = (jobject)cmd->data;
			avctx->opaque = (void*)0;
		}else if(ANDROID_DECODER_FLUSH_CMD == cmd->type ){
			 //av_log(avctx, AV_LOG_DEBUG,"%s: flush decode buffer command", __FUNCTION__);
			 Frame *frame;
			  ///
			 pthread_mutex_lock(&s->in_mutex);
	         while (!s->in_queue->empty()) {
	         	frame = *s->in_queue->begin();
	        	s->in_queue->erase(s->in_queue->begin());
	        	if (frame->size)
	            	av_freep(&frame->buffer);
	        		av_freep(&frame);
	    		}
	         pthread_mutex_unlock(&s->in_mutex);
			 ///
			 pthread_mutex_lock(&s->out_mutex);
	         while (!s->out_queue->empty()) {
	             frame = *s->out_queue->begin();
	             s->out_queue->erase(s->out_queue->begin());
	             if (frame->vframe)
	                 av_frame_free(&frame->vframe);
	             av_freep(&frame);
	         }
	         pthread_mutex_unlock(&s->out_mutex);
			 ///
			 pthread_mutex_lock(&s->time_map_mutex);
			 s->ts_map->clear();
			 pthread_mutex_unlock(&s->time_map_mutex);

			 //s->flush_cmd = (int)cmd->data;
			 
			 avctx->opaque = (void*)0;

		}
		
	}else{
		 //av_log(avctx, AV_LOG_DEBUG,"%s: flush decode buffer", __FUNCTION__);
		 Frame *frame;
		  ///
		 pthread_mutex_lock(&s->in_mutex);
         while (!s->in_queue->empty()) {
         	frame = *s->in_queue->begin();
        	s->in_queue->erase(s->in_queue->begin());
        	if (frame->size)
            	av_freep(&frame->buffer);
        		av_freep(&frame);
    		}
         pthread_mutex_unlock(&s->in_mutex);
		 ///
		 pthread_mutex_lock(&s->out_mutex);
         while (!s->out_queue->empty()) {
             frame = *s->out_queue->begin();
             s->out_queue->erase(s->out_queue->begin());
             if (frame->vframe)
                 av_frame_free(&frame->vframe);
             av_freep(&frame);
         }
         pthread_mutex_unlock(&s->out_mutex);
		 ///
		 pthread_mutex_lock(&s->time_map_mutex);
		 s->ts_map->clear();
		 pthread_mutex_unlock(&s->time_map_mutex);

		 //s->flush_cmd = 1;
		
	}

	//av_log(avctx, AV_LOG_ERROR,"%s: out", __FUNCTION__);
}

AVCodec ff_libstagefright_h264_decoder = {
    "libstagefright_h264",
    NULL_IF_CONFIG_SMALL("libstagefright_h264"),
    AVMEDIA_TYPE_VIDEO,
    AV_CODEC_ID_H264,
    CODEC_CAP_DELAY,
    NULL, //supported_framerates
    NULL, //pix_fmts
    NULL, //supported_samplerates
    NULL, //sample_fmts
    NULL, //channel_layouts
    0,    //max_lowres
    NULL, //priv_class
    NULL, //profiles
    sizeof(StagefrightContext),
    NULL, //next
    NULL, //init_thread_copy
    NULL, //update_thread_context
    NULL, //defaults
    NULL, //init_static_data
    Stagefright_init,
    NULL, //encode
    NULL, //encode2
    Stagefright_decode_frame,
    Stagefright_close,
    Stagefright_flush,
};
