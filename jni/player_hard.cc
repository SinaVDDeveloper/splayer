#include "player.h"
#include "player_log.h"
#include "audio_device_opensles.h"



int MediaPlayerHard::send_decoder_cmd( AVCodecContext *conetxt, int ctype, void* cdata)
{
	int ret = 0;
	decoder_cmd cmd;
	cmd.type_ = ctype;
	cmd.data_ = cdata;
	
	conetxt->opaque = (void*)&cmd;
	conetxt->codec->flush(conetxt);
	if( -1==((long)conetxt->opaque) ){
		ret = -1;
	}
	conetxt->opaque = NULL;

	return ret;
}
//static int MediaPlayerHard::packet_queue_put(PacketQueue *q, AVPacket *pkt);

int MediaPlayerHard::packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList *pkt1;

    if (q->abort_request)
       return -1;

    pkt1 = (MyAVPacketList*)av_malloc(sizeof(MyAVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    if (pkt == &this->flush_pkt)
        q->serial++;
    pkt1->serial = q->serial;

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case */
    LYH_SignalCond(q->cond); 
    return 0;
}

int MediaPlayerHard::packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;

    /* duplicate the packet */
    if (pkt != &this->flush_pkt && av_dup_packet(pkt) < 0)
        return -1;

    LYH_LockMutex(q->mutex); 
    ret = packet_queue_put_private(q, pkt);
    LYH_UnlockMutex(q->mutex); 

    if (pkt != &this->flush_pkt && ret < 0)
        av_free_packet(pkt);

    return ret;
}

int MediaPlayerHard::packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

/* packet queue handling */
void MediaPlayerHard::packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = LYH_CreateMutex();
    q->cond = LYH_CreateCond();
    q->abort_request = 1;
}

void MediaPlayerHard::packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    LYH_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    LYH_UnlockMutex(q->mutex);
}

void MediaPlayerHard::packet_queue_destroy(PacketQueue *q)
{
	if(q->mutex!=NULL && q->cond != NULL){   //new add
    	packet_queue_flush(q);
    	LYH_DestroyMutex(q->mutex);  //new add
		q->mutex = NULL;
    	LYH_DestroyCond(q->cond); 
		q->cond= NULL; //new add
	}
}

void MediaPlayerHard::packet_queue_abort(PacketQueue *q)
{
    LYH_LockMutex(q->mutex); 

    q->abort_request = 1;

    LYH_SignalCond(q->cond);

    LYH_UnlockMutex(q->mutex);
}

void MediaPlayerHard::packet_queue_start(PacketQueue *q)
{
    LYH_LockMutex(q->mutex); 
    q->abort_request = 0;
    packet_queue_put_private(q, &this->flush_pkt);
    LYH_UnlockMutex(q->mutex); 
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
//out: pkt ,serial
int  MediaPlayerHard::packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    MyAVPacketList *pkt1;
    int ret = 0;

    LYH_LockMutex(q->mutex); 

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            *pkt = pkt1->pkt;
            if (serial)
                *serial = pkt1->serial;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            LYH_WaitCond(q->cond, q->mutex); 
        }
    }
    LYH_UnlockMutex(q->mutex);
    return ret;
}




#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
    unsigned int v = ((const uint32_t *)(s))[0];\
    a = (v >> 24) & 0xff;\
    r = (v >> 16) & 0xff;\
    g = (v >> 8) & 0xff;\
    b = v & 0xff;\
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
    unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
    a = (val >> 24) & 0xff;\
    y = (val >> 16) & 0xff;\
    u = (val >> 8) & 0xff;\
    v = val & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
    ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}

void MediaPlayerHard::blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh)
{
    int wrap, wrap3, width2, skip2;
    int y, u, v, a, u1, v1, a1, w, h;
    uint8_t *lum, *cb, *cr;
    const uint8_t *p;
    const uint32_t *pal;
    int dstx, dsty, dstw, dsth;

    dstw = av_clip(rect->w, 0, imgw);
    dsth = av_clip(rect->h, 0, imgh);
    dstx = av_clip(rect->x, 0, imgw - dstw);
    dsty = av_clip(rect->y, 0, imgh - dsth);
    lum = dst->data[0] + dsty * dst->linesize[0];
    cb  = dst->data[1] + (dsty >> 1) * dst->linesize[1];
    cr  = dst->data[2] + (dsty >> 1) * dst->linesize[2];

    width2 = ((dstw + 1) >> 1) + (dstx & ~dstw & 1);
    skip2 = dstx >> 1;
    wrap = dst->linesize[0];
    wrap3 = rect->pict.linesize[0];
    p = rect->pict.data[0];
    pal = (const uint32_t *)rect->pict.data[1];  /* Now in YCrCb! */

    if (dsty & 1) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            p++;
            lum++;
        }
        p += wrap3 - dstw * BPP;
        lum += wrap - dstw - dstx;
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    for (h = dsth - (dsty & 1); h >= 2; h -= 2) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            p += wrap3;
            lum += wrap;

            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);

            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 2);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 2);

            cb++;
            cr++;
            p += -wrap3 + 2 * BPP;
            lum += -wrap + 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        p += wrap3 + (wrap3 - dstw * BPP);
        lum += wrap + (wrap - dstw - dstx);
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    /* handle odd height */
    if (h) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
        }
    }
}

void MediaPlayerHard::free_picture(VideoPicture *vp)
{
     if (vp->bmp) {
         LYH_FreeYUVOverlay(vp->bmp);
         vp->bmp = NULL;
     }
}

void MediaPlayerHard::free_subpicture(SubPicture *sp)
{
    avsubtitle_free(&sp->sub);
}


///将VideoState视频源中的pictq队列中一VideoPicture成员显示,显示前加上字幕
void MediaPlayerHard::video_image_display()
{
    VideoPicture *vp;
    SubPicture *sp;
    AVPicture pict;
    LYH_Rect rect;
    int i;

    vp = &this->pictq[this->pictq_rindex];
    if (vp->bmp) {
        if (this->subtitle_st) {
            if (this->subpq_size > 0) {
                sp = &this->subpq[this->subpq_rindex];

                if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000)) {
                    LYH_LockYUVOverlay (vp->bmp);

                    pict.data[0] = vp->bmp->pixels[0];
                    pict.data[1] = vp->bmp->pixels[1]; ///pict.data[1] = vp->bmp->pixels[2];
                    pict.data[2] = vp->bmp->pixels[2]; //pict.data[2] = vp->bmp->pixels[1];

                    pict.linesize[0] = vp->bmp->pitches[0];
                    pict.linesize[1] = vp->bmp->pitches[1]; //pict.linesize[1] = vp->bmp->pitches[2];
                    pict.linesize[2] = vp->bmp->pitches[2]; //pict.linesize[2] = vp->bmp->pitches[1];

                    for (i = 0; i < sp->sub.num_rects; i++)
                        blend_subrect(&pict, sp->sub.rects[i],
                                      vp->bmp->w, vp->bmp->h);

                    LYH_UnlockYUVOverlay (vp->bmp);
                }
            }
        }
		
        LYH_DisplayYUVOverlay(vp->bmp, &rect);

    }
}


///重绘最后一帧
void MediaPlayerHard::video_redisplay()
{
    VideoPicture *vp;
    SubPicture *sp;
    AVPicture pict;
    LYH_Rect rect;
    int i,ret;

	av_log(NULL, AV_LOG_ERROR,"%s: into\n",__FUNCTION__);

	if (!this->video_st) {
		av_log(NULL, AV_LOG_ERROR, "%s: no video stream\n",__FUNCTION__);
		return;
	}
	
	if (!this->screen){
		av_log(NULL, AV_LOG_ERROR, "%s: video not open yet\n",__FUNCTION__);
		return;
	}
		
    vp = &this->pictq[this->pictq_rindex];
    if (vp->allocated && vp->bmp) {
        if (this->subtitle_st) {
            //if (is->subpq_size > 0) {
                sp = &this->subpq[this->subpq_rindex];

                if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000)) {
                    LYH_LockYUVOverlay (vp->bmp);

                    pict.data[0] = vp->bmp->pixels[0];
                    pict.data[1] = vp->bmp->pixels[1]; 
                    pict.data[2] = vp->bmp->pixels[2]; 

                    pict.linesize[0] = vp->bmp->pitches[0];
                    pict.linesize[1] = vp->bmp->pitches[1]; 
                    pict.linesize[2] = vp->bmp->pitches[2];

                    for (i = 0; i < sp->sub.num_rects; i++)
                        blend_subrect(&pict, sp->sub.rects[i],vp->bmp->w, vp->bmp->h);

                    LYH_UnlockYUVOverlay (vp->bmp);
                }
            //}
        }
		
        LYH_ReDisplayYUVOverlay(vp->bmp); 

    }
}

void MediaPlayerHard::stream_close()
{
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

    int i;
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    if(!this->abort_request)
		this->abort_request = 1;
	/////////////////////可能还没创建!
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: wait read thred exit(%ld)\n",__FUNCTION__,this->read_tid);
    LYH_WaitThread(this->read_tid, NULL);
	this->read_tid = 0;
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: wait read thred exit end\n",__FUNCTION__);
    
    packet_queue_destroy(&this->videoq);
    packet_queue_destroy(&this->audioq);
    packet_queue_destroy(&this->subtitleq);

    /* free all pictures */
    for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++){
        free_picture(&this->pictq[i]);
    }
    for (i = 0; i < SUBPICTURE_QUEUE_SIZE; i++){
        free_subpicture(&this->subpq[i]);
    }
    if(this->pictq_mutex != NULL){
		LYH_DestroyMutex(this->pictq_mutex); 
		this->pictq_mutex = NULL;
    }
	if(this->pictq_cond != NULL){
    	LYH_DestroyCond(this->pictq_cond);
		this->pictq_cond = NULL;
	}
	if(this->subpq_mutex != NULL){
    	LYH_DestroyMutex(this->subpq_mutex);
		this->subpq_mutex = NULL;
	}
	if(this->subpq_cond != NULL){
    	LYH_DestroyCond(this->subpq_cond); 
		this->subpq_cond = NULL;
	}
	if(this->continue_read_thread != NULL){
    	LYH_DestroyCond(this->continue_read_thread);
		this->continue_read_thread = NULL;
	}
	if(this->img_convert_ctx=NULL){
    	sws_freeContext(this->img_convert_ctx);
		this->img_convert_ctx = NULL;
	}

	if(this->screen){
		av_log(NULL, AV_LOG_WARNING, "%s: close screen\n",__FUNCTION__);
		LYH_CloseSurface(this->screen);  ///new add here
		this->screen = NULL;
	}
	if(this->filename){
		av_free(this->filename);
		this->filename = NULL;
	}
	if(NULL!=this->hardware_context && NULL!=this->hardware_context->lyh_event_queue_mtx){
		LYH_DestroyMutex(this->hardware_context->lyh_event_queue_mtx);
		this->hardware_context->lyh_event_queue_mtx = NULL;
	}
	if(this->hardware_context){
		delete this->hardware_context;
		this->hardware_context = NULL;
	}

	if(mp_listener_lock){
		LYH_DestroyMutex(this->mp_listener_lock);
		this->mp_listener_lock = NULL;
	}
	
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
}

void MediaPlayerHard::do_exit(int destroy_vs)
{
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

	if(!this->abort_request)
		this->abort_request = 1; //move from stream_close() to here
    int wait_cnt = 0;
    while(this->stream_open_thread_id != 0){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: wait stream_open_thread quit!\n",__FUNCTION__);
		av_usleep(10000);
		wait_cnt++;
		if(wait_cnt>=500){
			av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_open_thread not quit yet!\n",__FUNCTION__);
			break;
		}
	}
	
    stream_close();
		/*
		if(destroy_vs){
			if(is->mediaplayer_listener){
				av_log(NULL, AV_LOG_WARNING, "%s: destroy mediaplayer mediaplayer_listener\n",__FUNCTION__);
				delete is->mediaplayer_listener;
				is->mediaplayer_listener = NULL;
			}

			//if(is->opengl2_render_listener){
			//	av_log(NULL, AV_LOG_WARNING, "%s: destroy mediaplayer opengl2_render_listener\n",__FUNCTION__);
			//	delete is->opengl2_render_listener;
			//	is->opengl2_render_listener = NULL;
			//}

			if(is->surface_render_listener){
				av_log(NULL, AV_LOG_WARNING, "%s: destroy mediaplayer surface_render_listener\n",__FUNCTION__);
				delete is->surface_render_listener;
				is->surface_render_listener = NULL;
			}
			
			av_log(NULL, AV_LOG_WARNING, "%s: destroy mediaplayer context\n",__FUNCTION__);
			av_free(is);
		}
		*/
    
    av_lockmgr_register(NULL);

    avformat_network_deinit();

	LYH_Quit(); 

  	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
}


//视频输出初始化
int MediaPlayerHard::video_open(int force_set_video_mode, VideoPicture *vp)
{
 
	if(NULL==this->screen){
		/*
		if(vp && vp->width)
			screen = LYH_OpenSurface(vp->width, vp->vp->height);
		else
			screen = LYH_OpenSurface(-1, -1);
			*/
		av_log(NULL, AV_LOG_INFO, "%s: LYH_OpenSurface begin\n",__FUNCTION__);
		this->screen =  LYH_OpenSurface(-1, -1, this->opengl2_render_listener );
		av_log(NULL, AV_LOG_INFO, "%s: LYH_OpenSurface end\n",__FUNCTION__);
		if(!this->screen){
			return -1;
		}
	}
	
	
	if(this->screen){
		this->screen->GetWidthHeight(this->width,this->height);
	}
    //is->width  = screen->w;
    //is->height = screen->h;

    return 0;
}

/* display the current picture, if any */
void MediaPlayerHard::video_display()
{
	int ret = 0;
    if (!this->screen){
		return;
        //ret = video_open(is, 0, NULL);
		//if(-1==ret){
		//	av_log(NULL, AV_LOG_ERROR, "%s: video_open fail\n",__FUNCTION__);
		//	return;
		//}
    }
    if (this->video_st)
        video_image_display(); 
}

double MediaPlayerHard::get_clock(Clock *c)
{
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

void MediaPlayerHard::set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

void MediaPlayerHard::set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

void MediaPlayerHard::set_clock_speed(Clock *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

void MediaPlayerHard::init_clock(Clock *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

void MediaPlayerHard::sync_clock_to_slave(Clock *c, Clock *slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

int MediaPlayerHard::get_master_sync_type() 
{
    if (this->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (this->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (this->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (this->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

/* get the current master clock value */
double MediaPlayerHard::get_master_clock()
{
    double val;

    switch (get_master_sync_type()) {
        case AV_SYNC_VIDEO_MASTER:
            val = get_clock(&this->vidclk);
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_clock(&this->audclk);
            break;
        default:
            val = get_clock(&this->audclk); ///val = get_clock(&is->extclk);
            break;
    }
    return val;
}
/*
static void check_external_clock_speed(MediaPlayer *is) {
   if (is->video_stream >= 0 && is->videoq.nb_packets <= MIN_FRAMES / 2 ||
       is->audio_stream >= 0 && is->audioq.nb_packets <= MIN_FRAMES / 2) {
       set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
   } else if ((is->video_stream < 0 || is->videoq.nb_packets > MIN_FRAMES * 2) &&
              (is->audio_stream < 0 || is->audioq.nb_packets > MIN_FRAMES * 2)) {
       set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
   } else {
       double speed = is->extclk.speed;
       if (speed != 1.0)
           set_clock_speed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
   }
}
*/
/* seek in the stream */
void MediaPlayerHard::stream_seek(int64_t pos, int64_t rel, int seek_by_bytes)
{
    if (!this->seek_req) {
        this->seek_pos = pos;
        this->seek_rel = rel;
        this->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            this->seek_flags |= AVSEEK_FLAG_BYTE;
        this->seek_req = 1;
        LYH_SignalCond(this->continue_read_thread); 
    }
}

/* pause or resume the video */
void MediaPlayerHard::stream_toggle_pause(int unpause)
{
    if (this->paused) {
        this->frame_timer += av_gettime() / 1000000.0 + this->vidclk.pts_drift - this->vidclk.pts;
        if (this->read_pause_return != AVERROR(ENOSYS)) {
            this->vidclk.paused = 0;
        }
        set_clock(&this->vidclk, get_clock(&this->vidclk), this->vidclk.serial);
    }
    ////set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
    //this->paused = this->audclk.paused = this->vidclk.paused = !this->paused; ///is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = !is->paused;
	if(unpause){
		if(this->paused){
			this->paused = this->audclk.paused = this->vidclk.paused = !this->paused; 
		}
	}else{
		this->paused = this->audclk.paused = this->vidclk.paused = !this->paused; 
	}
	//av_log(NULL, AV_LOG_DEBUG,"%s: is->paused=%d",__FUNCTION__,is->paused);  
}

void MediaPlayerHard::toggle_pause()
{
    stream_toggle_pause(0);
    this->step = 0;
}

void MediaPlayerHard::step_to_next_frame()
{
    /* if the stream is paused unpause it, then step */
    //if (this->paused)
    //    stream_toggle_pause();
    stream_toggle_pause(1);
    this->step = 1;
}
///
double MediaPlayerHard::compute_target_delay(double delay)
{
    double sync_threshold, diff,olddelay;
	olddelay = delay;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type() != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_clock(&this->vidclk) - get_master_clock();

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < this->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }

		//if(!isnan(diff) && diff>=1.0){
		//	av_log(NULL, AV_LOG_ERROR, "%s: diff to big, A-V=%f\n",__FUNCTION__,-diff);
		//}
    }

	
    //av_log(NULL, AV_LOG_DEBUG, "%s: video: olddelay=%0.3f,delay=%0.3f A-V=%f\n",
    //        __FUNCTION__,olddelay,delay, -diff);

    return delay;
}

void MediaPlayerHard::pictq_next_picture() 
{
    /* update queue size and signal for next picture */
    if (++this->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
        this->pictq_rindex = 0;

    LYH_LockMutex(this->pictq_mutex);
    this->pictq_size--;
    LYH_SignalCond(this->pictq_cond);
    LYH_UnlockMutex(this->pictq_mutex);
}

int MediaPlayerHard::pictq_prev_picture()
{
    VideoPicture *prevvp;
    int ret = 0;
    /* update queue size and signal for the previous picture */
    prevvp = &this->pictq[(this->pictq_rindex + VIDEO_PICTURE_QUEUE_SIZE - 1) % VIDEO_PICTURE_QUEUE_SIZE];
    if (prevvp->allocated && prevvp->serial == this->videoq.serial) {
        LYH_LockMutex(this->pictq_mutex);
        if (this->pictq_size < VIDEO_PICTURE_QUEUE_SIZE) {
            if (--this->pictq_rindex == -1)
                this->pictq_rindex = VIDEO_PICTURE_QUEUE_SIZE - 1;
            this->pictq_size++;
            ret = 1;
        }
        LYH_SignalCond(this->pictq_cond); 
        LYH_UnlockMutex(this->pictq_mutex);
    }
    return ret;
}

void MediaPlayerHard::update_video_pts(double pts, int64_t pos, int serial) 
{
	av_log(NULL, AV_LOG_INFO,"%s: into, pts=%lf,pos=%lld,serial=%d\n",__FUNCTION__,pts,pos,serial);

	/* update current video pts */
    set_clock(&this->vidclk, pts, serial);
    ////sync_clock_to_slave(&is->extclk, &is->vidclk);
    this->video_current_pos = pos;
    this->frame_last_pts = pts;

	this->frame_last_serial = serial;
}

/* called to display each frame */
void MediaPlayerHard::video_refresh(double *remaining_time)
{
    //MediaPlayer *is = (MediaPlayer *)opaque;
    double time;
	int ret=0;

	//av_log(NULL, AV_LOG_ERROR,"%s: into\n",__FUNCTION__);

    if (this->video_st) {
            double last_duration, duration, delay;
			
			if (!this->buffer_full){
				av_log(NULL, AV_LOG_DEBUG,"%s: buffer not full!\n",__FUNCTION__);
				return;
			}

            if (this->paused){
				av_log(NULL, AV_LOG_DEBUG,"%s: pause!\n",__FUNCTION__);
                return;
            }

			///hard decode, just got a output buffer and update video timer
			///should got next buffer until no av time diff!
retry:
			int got_picture=0;
			if(this->video_decode_frame==NULL){
				this->video_decode_frame = av_frame_alloc();
				this->video_decode_frame_show = 1;
			}
			//av_log(NULL, AV_LOG_ERROR,"%s: this->video_decode_frame_show=%d\n",__FUNCTION__,this->video_decode_frame_show);
			if(this->video_decode_frame_show){
				avcodec_get_frame_defaults(this->video_decode_frame);
				ret =  this->video_st->codec->codec->decode(this->video_st->codec, this->video_decode_frame, &got_picture, NULL);// avcodec_decode_video2(is->video_st->codec, is->video_decode_frame, &got_picture, NULL); 
				if(ret<0){
					av_log(NULL, AV_LOG_ERROR,"%s: decoder decode frame catch error!\n",__FUNCTION__);
					///should info mediaplayer listener
					notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED ,MEDIA_ERROR_DECODER_FAIL, NULL);
					return;
				}
				if(got_picture){
					/////just debug
					//av_log(NULL, AV_LOG_ERROR, "%s:this->video_decode_frame frame->pts=%lld,frame->pkt_pts=%lld,frame->pkt_pos=%lld\n",
					//	__FUNCTION__,this->video_decode_frame->pts,this->video_decode_frame->pkt_pts,this->video_decode_frame->pkt_pos);
					/////
					///seek后不显示以前的图片
					if(this->video_decode_frame->display_picture_number!=this->videoq.serial){
						//av_log(NULL, AV_LOG_ERROR,"%s: not update old pic, is->video_decode_frame->display_picture_number=%d,is->videoq.serial=%d\n",__FUNCTION__,this->video_decode_frame->display_picture_number,this->videoq.serial);
						goto retry;
					}
					this->video_decode_frame_show = 0;
				}else{
					//av_log(NULL, AV_LOG_ERROR,"%s: no deocde picture, just retur\n",__FUNCTION__);
					return;
				}
			}
			
			if(this->video_decode_frame_show==0){
				double cur_pts = (this->video_decode_frame->pts == AV_NOPTS_VALUE) ? NAN : this->video_decode_frame->pts * av_q2d(this->video_st->time_base);
				int64_t cur_pos = av_frame_get_pkt_pos(this->video_decode_frame);
				int cur_serial = this->video_decode_frame->display_picture_number;

				if(this->frame_last_serial == cur_serial){
					//last_duration = cur_pts - this->frame_last_pts;
					if( this->video_decode_frame->pts!=AV_NOPTS_VALUE && this->frame_last_pts!=AV_NOPTS_VALUE){
						last_duration = cur_pts - this->frame_last_pts;
					}
					
	          		if (!isnan(last_duration) && last_duration > 0 && last_duration < this->max_frame_duration) {
	                	/* if duration of the last frame was sane(not crazy), update last_duration in MediaPlayer */
	                	this->frame_last_duration = last_duration;
	            	}

					delay = compute_target_delay(this->frame_last_duration);

	            	time = av_gettime()/1000000.0;

					//av_log(NULL, AV_LOG_ERROR,"%s:cur_pts=%lf,cur_serial=%d,frame_last_pts=%lf,frame_last_duration=%lf,delay=%lf,is->frame_timer=%lf,time=%lf\n",
					//		__FUNCTION__,cur_pts,cur_serial,this->frame_last_pts,this->frame_last_duration,delay,this->frame_timer,time);

					if (time < this->frame_timer + delay) {
	                	*remaining_time = FFMIN(this->frame_timer + delay - time, *remaining_time);
						//av_log(NULL, AV_LOG_ERROR,"%s: render frame early! remaining_time=%lf\n",__FUNCTION__,*remaining_time);
	                	return;
	            	}
				
	            	this->frame_timer += delay;
	            	if (delay > 0 && time - this->frame_timer > AV_SYNC_THRESHOLD_MAX){
	                	this->frame_timer = time;
	            	}
				}else{
					//av_log(NULL, AV_LOG_ERROR,"%s: serial not the same! this->frame_last_serial=%d,cur_serial=%d\n",
					//	__FUNCTION__,this->frame_last_serial,cur_serial);
					
					time = av_gettime()/1000000.0;
					this->frame_timer += this->frame_last_duration;
	            	if (time - this->frame_timer > AV_SYNC_THRESHOLD_MAX){
	                	this->frame_timer = time;
	            	}
				}
			
				///更新视频时钟
				LYH_LockMutex(this->pictq_mutex);
            	if (!isnan(cur_pts)){
                	update_video_pts(cur_pts, cur_pos, cur_serial); //update_video_pts(cur_pts, cur_pos, this->videoq.serial);
            	}
				LYH_UnlockMutex(this->pictq_mutex);

				this->video_decode_frame_show = 1;
				
			}
    }
    // is->force_refresh = 0;
    if (0) { //if (show_status) {
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize;
        double av_diff;

        cur_time = av_gettime();
        if (!last_time || (cur_time - last_time) >= 30000) {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (this->audio_st)
                aqsize = this->audioq.size;
            if (this->video_st)
                vqsize = this->videoq.size;
            if (this->subtitle_st)
                sqsize = this->subtitleq.size;
            av_diff = 0;
            if (this->audio_st && this->video_st)
                av_diff = get_clock(&this->audclk) - get_clock(&this->vidclk);
            else if (this->video_st)
                av_diff = get_master_clock() - get_clock(&this->vidclk);
            else if (this->audio_st)
                av_diff = get_master_clock() - get_clock(&this->audclk);
            av_log(NULL, AV_LOG_INFO,
                   "%s: mc:%7.2f %s:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%lld:%lld   \n",
                   __FUNCTION__,
                   get_master_clock(),
                   (this->audio_st && this->video_st) ? "A-V" : (this->video_st ? "M-V" : (this->audio_st ? "M-A" : "   ")),
                   av_diff,
                   this->frame_drops_early + this->frame_drops_late,
                   aqsize / 1024,
                   vqsize / 1024,
                   sqsize,
                   this->video_st ? this->video_st->codec->pts_correction_num_faulty_dts : 0,
                   this->video_st ? this->video_st->codec->pts_correction_num_faulty_pts : 0);
            ///fflush(stdout);
            last_time = cur_time;
        }
    }
}

/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
int MediaPlayerHard::alloc_picture()
{
    VideoPicture *vp;
    int64_t bufferdiff;

	av_log(NULL, AV_LOG_INFO, "%s: into\n",__FUNCTION__);

    vp = &this->pictq[this->pictq_windex];

    free_picture(vp);

	video_open(0, vp);///video_open(0, vp);

    vp->bmp = LYH_CreateYUVOverlay(vp->width, vp->height, 
                                   AV_PIX_FMT_YUV420P,
                                   this->screen);

	///av_log(NULL, AV_LOG_INFO, "%s: step 1\n",__FUNCTION__);
    bufferdiff = vp->bmp ? FFMAX(vp->bmp->pixels[0], vp->bmp->pixels[1]) - FFMIN(vp->bmp->pixels[0], vp->bmp->pixels[1]) : 0;
    if (!vp->bmp || vp->bmp->pitches[0] < vp->width || bufferdiff < (int64_t)vp->height * vp->bmp->pitches[0]) {
        /* allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        av_log(NULL, AV_LOG_FATAL,"Error: the video system does not support an image size of %dx%d pixels.\n", vp->width, vp->height );
        //do_exit(is);
		return -1;
    }

	///av_log(NULL, AV_LOG_INFO, "%s: step 2\n",__FUNCTION__);
    LYH_LockMutex(this->pictq_mutex); 
    vp->allocated = 1;
    LYH_SignalCond(this->pictq_cond); 
    LYH_UnlockMutex(this->pictq_mutex); 

	av_log(NULL, AV_LOG_INFO, "%s: out\n",__FUNCTION__);

	return 0;
}
///
/*
static void duplicate_right_border_pixels(LYH_Overlay *bmp) { 
    unsigned char *p, *maxp; ///Uint8 *p, *maxp;
    for (i = 0; i < 3; i++) {
        width  = bmp->w;
        height = bmp->h;
        if (i > 0) {
            width  >>= 1;
            height >>= 1;
        }
        if (bmp->pitches[i] > width) {
            maxp = bmp->pixels[i] + bmp->pitches[i] * height - 1;
            for (p = bmp->pixels[i] + width - 1; p < maxp; p += bmp->pitches[i])
                *(p+1) = *p;
        }
    }
}
*/
///move from cmd.cc
//static struct SwsContext *sws_opts;
///
///将frame放进队列 MediaPlayer->pictq[], VideoPicture数组
int  MediaPlayerHard::queue_picture(AVFrame *src_frame, double pts, int64_t pos, int serial)
{
    VideoPicture *vp;

	///av_log(NULL, AV_LOG_INFO, "%s: into\n",__FUNCTION__);

///#if defined(DEBUG_SYNC) && 0
    //av_log(NULL, AV_LOG_INFO,"%s: frame_type=%c pts=%0.3f pos=%ld serial=%d\n",///printf("frame_type=%c pts=%0.3f\n",
    //       __FUNCTION__,av_get_picture_type_char(src_frame->pict_type), pts, pos,serial);
///#endif

    /* wait until we have space to put a new picture */
    LYH_LockMutex(this->pictq_mutex);

    /* keep the last already displayed picture in the queue */
    while (this->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE - 1 &&
           !this->videoq.abort_request) {
        LYH_WaitCond(this->pictq_cond, this->pictq_mutex); 
    }
    LYH_UnlockMutex(this->pictq_mutex);

	///av_log(NULL, AV_LOG_INFO, "%s: step 1\n",__FUNCTION__);

    if (this->videoq.abort_request){
        return -1;
    }

    vp = &this->pictq[this->pictq_windex];

    vp->sar = src_frame->sample_aspect_ratio;

    /* alloc or resize hardware picture buffer */
    if (!vp->bmp || vp->reallocate || !vp->allocated ||
        vp->width  != src_frame->width ||
        vp->height != src_frame->height) {
        LYH_event event; 

        vp->allocated  = 0;
        vp->reallocate = 0;
        vp->width = src_frame->width;
        vp->height = src_frame->height;
		this->frame_width = src_frame->width;
		this->frame_height = src_frame->height;

        /* the allocation must be done in the main thread to avoid
           locking problems. */
        event.type = LYH_ALLOC_EVENT;
        event.data = this; ///event.user.data1 = is;

		///av_log(NULL, AV_LOG_INFO, "%s: LYH_PushEvent begin\n",__FUNCTION__);
        LYH_PushEvent(this->hardware_context,&event);

        /* wait until the picture is allocated */
        LYH_LockMutex(this->pictq_mutex);
        while (!vp->allocated && !this->videoq.abort_request) {
            LYH_WaitCond(this->pictq_cond, this->pictq_mutex); 
        }
        /* if the queue is aborted, we have to pop the pending ALLOC event or wait for the allocation to complete */
        if (this->videoq.abort_request && LYH_PeepEvents(this->hardware_context,&event,LYH_ALLOC_EVENT) != 1) { 
            while (!vp->allocated) {
                LYH_WaitCond(this->pictq_cond, this->pictq_mutex);
            }
        }
        LYH_UnlockMutex(this->pictq_mutex); 

        if (this->videoq.abort_request){
            return -1;
        }
    }

	///av_log(NULL, AV_LOG_INFO, "%s: step 2\n",__FUNCTION__);

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
        AVPicture pict = { { 0 } };

        /* get a pointer on the bitmap */
        LYH_LockYUVOverlay (vp->bmp); 

        pict.data[0] = vp->bmp->pixels[0];
        pict.data[1] = vp->bmp->pixels[1]; //pict.data[1] = vp->bmp->pixels[2];
        pict.data[2] = vp->bmp->pixels[2]; //pict.data[2] = vp->bmp->pixels[1];
		///YUV: linesize[0] =  width + padding size(16+16),linesize[1]=linesize[0]/2,linesize[2]=linesize[0]/2
        pict.linesize[0] = vp->bmp->pitches[0];
        pict.linesize[1] = vp->bmp->pitches[1]; //pict.linesize[1] = vp->bmp->pitches[2];
        pict.linesize[2] = vp->bmp->pitches[2]; //pict.linesize[2] = vp->bmp->pitches[1];

		///av_log(NULL, AV_LOG_INFO, "%s: sws_getCachedContext begin\n",__FUNCTION__);
		/*
        av_opt_get_int(sws_opts, "sws_flags", 0, &sws_flags);
        is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
            vp->width, vp->height, (enum AVPixelFormat)src_frame->format, vp->width, vp->height,
            AV_PIX_FMT_YUV420P, sws_flags, NULL, NULL, NULL);
        if (is->img_convert_ctx == NULL) {
            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
            return -1;
        }
		av_log(NULL, AV_LOG_INFO,"%s: src_frame->linesize[0]=%d,[1]=%d,[2]=%d,width=%d,height=%d\n",__FUNCTION__,src_frame->linesize[0],src_frame->linesize[1],src_frame->linesize[2],src_frame->width,src_frame->height);
        sws_scale(is->img_convert_ctx, src_frame->data, src_frame->linesize, 0, vp->height, pict.data, pict.linesize); ///sws_scale来进行图像缩放和格式转换，该函数可以使用各种不同算法来对图像进行处理
		*/
		//av_log(NULL, AV_LOG_INFO,"%s: hi,src_frame->linesize[0]=%d,[1]=%d,[2]=%d,width=%d,height=%d\n",__FUNCTION__,src_frame->linesize[0],src_frame->linesize[1],src_frame->linesize[2],src_frame->width,src_frame->height);
		if(src_frame->linesize[0]==src_frame->width){
			memcpy(pict.data[0],src_frame->data[0],src_frame->width*src_frame->height);
			memcpy(pict.data[1],src_frame->data[1],src_frame->width*src_frame->height/4);
			memcpy(pict.data[2],src_frame->data[2],src_frame->width*src_frame->height/4);
		}else{
			//av_log(NULL, AV_LOG_INFO,"%s: pading data\n",__FUNCTION__);
			///Y
			uint8_t* pDst = pict.data[0];
			uint8_t* pSrc = src_frame->data[0];
			for (int i = 0; i < src_frame->height; i++)
        	{
            	memcpy(pDst,pSrc,src_frame->width);
            	pSrc += src_frame->linesize[0]; // iLineSize padding
            	pDst += src_frame->width;
        	}
			///U
			pDst = pict.data[1];
			pSrc = src_frame->data[1];
			for (int i = 0; i < src_frame->height/2; i++)
        	{
            	memcpy(pDst,pSrc,src_frame->width/2);
            	pSrc += src_frame->linesize[1];
            	pDst += src_frame->width/2;
        	}
			///V
			pDst = pict.data[2];
			pSrc = src_frame->data[2];
			for (int i = 0; i < src_frame->height/2; i++)
        	{
            	memcpy(pDst,pSrc,src_frame->width/2);
            	pSrc += src_frame->linesize[2]; 
            	pDst += src_frame->width/2;
        	}
		
		}
		
        /* workaround SDL PITCH_WORKAROUND workaround工作区；变通方案；权变措施*/
        ////////duplicate_right_border_pixels(vp->bmp); ///可以省略
        /* update the bitmap content */
		
        LYH_UnlockYUVOverlay(vp->bmp);

		///av_log(NULL, AV_LOG_INFO, "%s: step 3\n",__FUNCTION__);

        vp->pts = pts;
        vp->pos = pos;
        vp->serial = serial;

        /* now we can update the picture count */
        if (++this->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
            this->pictq_windex = 0;
        LYH_LockMutex(this->pictq_mutex);
        this->pictq_size++;
        LYH_UnlockMutex(this->pictq_mutex); 
    }
    return 0;
}


int MediaPlayerHard::get_video_frame(AVFrame *frame, AVPacket *pkt, int *serial)
{
    int got_picture = 0, ret;

	//av_log(NULL, AV_LOG_INFO, "%s: into\n",__FUNCTION__);
	///play MP3 bug fix
	if(this->abort_request){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: is->abort_request\n",__FUNCTION__);
		return -1;
	}
	
	if(this->videoq.nb_packets<=0 && !this->eof && this->audioq.nb_packets< this->MAX_FRAMES ){ //if(is->videoq.size<=0 && !is->eof && is->audioq.size< is->MAX_FRAMES ){ //if(is->videoq.size<=0){
		if( this->buffer_full ){
			this->buffer_full = 0;
			notify(MEDIA_INFO, MEDIA_INFO_BUFFERING_START,0 , NULL);
			av_log(NULL, AV_LOG_DEBUG, "%s: is->videoq.nb_packets<=0,set is->buffer_full = 0\n",__FUNCTION__);
		}
		return 0;
	}
	///获得一个pkt
	//av_log(NULL, AV_LOG_ERROR, "%s: packet_queue_get into\n",__FUNCTION__);
	ret = packet_queue_get(&this->videoq, pkt, 1, serial);
	//av_log(NULL, AV_LOG_ERROR, "%s: packet_queue_get out\n",__FUNCTION__);
    if (ret < 0){
		av_log(NULL, AV_LOG_DEBUG, "%s: packet_queue_get return < 0\n",__FUNCTION__);
        return -1;
    }

    if (pkt->data == this->flush_pkt.data) { ///seek 才会发生?
        avcodec_flush_buffers(this->video_st->codec);

        LYH_LockMutex(this->pictq_mutex);
        // Make sure there are no long delay timers (ideally we should just flush the queue but that's harder)
        while (this->pictq_size && !this->videoq.abort_request) {
            LYH_WaitCond(this->pictq_cond, this->pictq_mutex);
        }
        this->video_current_pos = -1;
        this->frame_last_pts = AV_NOPTS_VALUE;
        this->frame_last_duration = 0;
        this->frame_timer = (double)av_gettime() / 1000000.0;
        this->frame_last_dropped_pts = AV_NOPTS_VALUE;
        LYH_UnlockMutex(this->pictq_mutex); 
        return 0;
    }
	
	///解码 pkt -> frame
	///skip frame
	if(!this->skip_auto_closed){
		if(this->frame_drop_step==FRAME_DROP_STEP_1){
			if(this->video_st->codec->skip_frame != AVDISCARD_NONREF){
				this->video_st->codec->skip_frame = AVDISCARD_NONREF;
			}
		}else if(this->frame_drop_step==FRAME_DROP_STEP_2){
			if(this->video_st->codec->skip_loop_filter != AVDISCARD_NONKEY){
				this->video_st->codec->skip_loop_filter = AVDISCARD_NONKEY;
				this->video_st->codec->skip_idct = AVDISCARD_NONKEY;
			}
		}
	}
	//av_log(NULL, AV_LOG_INFO, "%s: avcodec_decode_video2 begin\n",__FUNCTION__);
decode:
	//double dec_time =  (double)av_gettime();   ////just for test
	this->video_st->codec->compression_level = *serial;
	//av_log(NULL, AV_LOG_ERROR, "%s: pkt->pts=%lld,pkt->dts=%lld,pkt->pos=%lld,pkt->duration=%d\n",__FUNCTION__,pkt->pts,pkt->dts,pkt->pos,pkt->duration);
	ret = avcodec_decode_video2(this->video_st->codec, frame, &got_picture, pkt); //ret = avcodec_decode_video2(is->video_st->codec, frame, &got_picture, pkt);
				
	if(ret == AVERROR_BUFFER_TOO_SMALL){
		av_log(NULL, AV_LOG_DEBUG, "%s: avcodec_decode_video2 return AVERROR_BUFFER_TOO_SMALL\n",__FUNCTION__);
		if(this->abort_request || this->videoq.abort_request){
			av_log(NULL, AV_LOG_DEBUG, "%s: is->videoq.abort_request=%d,is->abort_request=%d\n",
				__FUNCTION__,this->videoq.abort_request,this->abort_request);
			return -1;
		}
		av_usleep(10000);
		goto  decode;
	}else if(ret < 0){
		av_log(NULL, AV_LOG_DEBUG, "%s: avcodec_decode_video2 return < 0\n",__FUNCTION__);
        return 0;
    }
	//double dec_time_end =  (double)av_gettime();  ////just for test
	//av_log(NULL, AV_LOG_WARNING, "%s: input a frame time=%lf\n",__FUNCTION__,(dec_time_end - dec_time)/1000000);
	//av_log(NULL, AV_LOG_INFO, "%s: avcodec_decode_video2 end,frame->pict_type=%d,got_picture=%d\n",__FUNCTION__,frame->pict_type,got_picture);

    //if (!got_picture && !pkt->data){
    //    is->video_finished = *serial;
    //}

	///解码成功
	return 1;
	
	/*
    if (0) {
        int ret = 1;
        double dpts = NAN;

        //if (decoder_reorder_pts == -1) {
            frame->pts = av_frame_get_best_effort_timestamp(frame);
        //} else if (decoder_reorder_pts) {
        //    frame->pts = frame->pkt_pts;
        //} else {
        //    frame->pts = frame->pkt_dts;
        //}

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(this->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(this->ic, this->video_st, frame);
		
        if (this->framedrop>0 || (this->framedrop && get_master_sync_type() != AV_SYNC_VIDEO_MASTER)) {
            LYH_LockMutex(this->pictq_mutex);
            if (this->frame_last_pts != AV_NOPTS_VALUE && frame->pts != AV_NOPTS_VALUE) {
                double clockdiff = get_clock(&this->vidclk) - get_master_clock();
                double ptsdiff = dpts - this->frame_last_pts;
                if (!isnan(clockdiff) && fabs(clockdiff) < AV_NOSYNC_THRESHOLD &&
                    !isnan(ptsdiff) && ptsdiff > 0 && ptsdiff < AV_NOSYNC_THRESHOLD &&
                    clockdiff + ptsdiff - this->frame_last_filter_delay < 0 &&
                    this->videoq.nb_packets) {
                    av_log(NULL, AV_LOG_WARNING, "%s: drop a frame early,clockdiff=%lf,ptsdiff=%lf,dpts=%lf,is->videoq.nb_packets=%d,is->frame_drops_early_continue=%d\n",
						__FUNCTION__,clockdiff,ptsdiff,dpts,this->videoq.nb_packets,this->frame_drops_early_continue);
                    this->frame_last_dropped_pos = av_frame_get_pkt_pos(frame);
                    this->frame_last_dropped_pts = dpts;
                    this->frame_last_dropped_serial = *serial;
                    this->frame_drops_early++;
					if(!this->skip_auto_closed){
						this->frame_drops_early_continue++;
						if(this->frame_drops_early_continue>FRAME_DROP_THRESHOLD){
							if(0==this->frame_drop_step){
								this->frame_drop_step = FRAME_DROP_STEP_1;
								av_log(NULL, AV_LOG_ERROR,"%s: FRAME_DROP_STEP_1\n",__FUNCTION__);
								this->frame_drops_early_continue = 0;
							}else if(FRAME_DROP_STEP_1==this->frame_drop_step){
								this->frame_drop_step = FRAME_DROP_STEP_2;
								av_log(NULL, AV_LOG_ERROR,"%s: FRAME_DROP_STEP_2\n",__FUNCTION__);
								this->frame_drops_early_continue = 0;
							}
						}
					}
                    ///av_frame_unref(frame);
                    ret = 0;
                }else{
					///NEW ADD no else
					if(!this->skip_auto_closed){
						this->frame_drops_early_continue = 0;
					}
				}
            }
            LYH_UnlockMutex(this->pictq_mutex);
        }

        return ret;
    }
	*/
    //return 0;
}



///video解码线程
void* MediaPlayerHard::video_thread(void *arg)
{
    AVPacket pkt = { 0 };
    MediaPlayerHard *is = (MediaPlayerHard *)arg;
    AVFrame *frame = av_frame_alloc();
    double pts;
    int ret;
    int serial = 0;

	av_log(NULL, AV_LOG_DEBUG, "%s: video thread into\n",__FUNCTION__);

    for (;;) {
        while (is->paused && !is->videoq.abort_request && !is->abort_request){
			av_log(NULL, AV_LOG_WARNING, "%s: in paused state\n",__FUNCTION__);
            LYH_Delay(10);
        }
		while(!is->buffer_full && !is->videoq.abort_request && !is->abort_request){
			av_log(NULL, AV_LOG_WARNING, "%s: buferr not full\n",__FUNCTION__);
			LYH_Delay(10);
		}

        avcodec_get_frame_defaults(frame);
        av_free_packet(&pkt);

		//获得一个解码后的frame,从is的videoq(PacketQueue)队列
        ret = is->get_video_frame(frame, &pkt, &serial);
        if (ret < 0){
			av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: get_video_frame return <0, quit video_thread\n",__FUNCTION__);
            goto the_end;
        }
        if (!ret){
			av_log(NULL, AV_LOG_WARNING, "%s: get_video_frame return 0\n",__FUNCTION__);
            continue;
        }

        //pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(is->video_st->time_base);

		//frame放进is里的队列去
		////不显示
		////ret = queue_picture(is, frame, pts, av_frame_get_pkt_pos(frame), serial);

		//////av_frame_unref(frame);

		//update_video_pts(is, pts, av_frame_get_pkt_pos(frame), serial); ///new add by lyh

		//double  av_clock_diff = get_clock(&is->vidclk) - get_master_clock(is);
		
		//if(av_clock_diff>0){
		//	av_log(NULL, AV_LOG_INFO, "%s: av_clock_diff=%lf\n",__FUNCTION__,av_clock_diff);
		//	av_usleep((int64_t)(av_clock_diff * 1000000.0));
		//}

        //if (ret < 0){
		//	av_log(NULL, AV_LOG_WARNING, "%s: queue_picture return <0, quit video_thread\n",__FUNCTION__);
        //    goto the_end;
        //}
    }
 the_end:
    avcodec_flush_buffers(is->video_st->codec);

    av_free_packet(&pkt);
    av_frame_free(&frame);
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: video thread exit\n",__FUNCTION__);
    return 0;
}

void* MediaPlayerHard::subtitle_thread(void *arg)
{
    MediaPlayerHard *is = (MediaPlayerHard *)arg;
    SubPicture *sp;
    AVPacket pkt1, *pkt = &pkt1;
    int got_subtitle;
    int serial;
    double pts;
    int i, j;
    int r, g, b, y, u, v, a;

	av_log(NULL, AV_LOG_INFO, "%s: subtitle thread into\n",__FUNCTION__);

    for (;;) {
        while (is->paused && !is->subtitleq.abort_request) {
            LYH_Delay(10);
        }
		//从is的subtitleq的PacketQueue队列获得一个包
        if (is->packet_queue_get(&is->subtitleq, pkt, 1, &serial) < 0)
            break;

        if (pkt->data == is->flush_pkt.data) {
            avcodec_flush_buffers(is->subtitle_st->codec);
            continue;
        }
        LYH_LockMutex(is->subpq_mutex); 
        while (is->subpq_size >= SUBPICTURE_QUEUE_SIZE &&
               !is->subtitleq.abort_request) {
           LYH_WaitCond(is->subpq_cond, is->subpq_mutex); 
        }
        LYH_UnlockMutex(is->subpq_mutex); 

        if (is->subtitleq.abort_request)
            return 0;

		///is的subpq队列
        sp = &is->subpq[is->subpq_windex];

       /* NOTE: ipts is the PTS of the _first_ picture beginning in
           this packet, if any */
        pts = 0;
        if (pkt->pts != AV_NOPTS_VALUE)
            pts = av_q2d(is->subtitle_st->time_base) * pkt->pts;

		///解码pkt->sub
        avcodec_decode_subtitle2(is->subtitle_st->codec, &sp->sub,
                                 &got_subtitle, pkt);
        if (got_subtitle && sp->sub.format == 0) {
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE;
            sp->pts = pts;
            sp->serial = serial;

            for (i = 0; i < sp->sub.num_rects; i++)
            {
                for (j = 0; j < sp->sub.rects[i]->nb_colors; j++)
                {
                    RGBA_IN(r, g, b, a, (uint32_t*)sp->sub.rects[i]->pict.data[1] + j);
                    y = RGB_TO_Y_CCIR(r, g, b);
                    u = RGB_TO_U_CCIR(r, g, b, 0);
                    v = RGB_TO_V_CCIR(r, g, b, 0);
                    YUVA_OUT((uint32_t*)sp->sub.rects[i]->pict.data[1] + j, y, u, v, a);
                }
            }

            /* now we can update the picture count */
            if (++is->subpq_windex == SUBPICTURE_QUEUE_SIZE)
                is->subpq_windex = 0;
            LYH_LockMutex(is->subpq_mutex); 
            is->subpq_size++;
            LYH_UnlockMutex(is->subpq_mutex);
        } else if (got_subtitle) {
            avsubtitle_free(&sp->sub);
        }
        av_free_packet(pkt);
    }

	av_log(NULL, AV_LOG_ERROR, "%s: subtitle thread exit\n",__FUNCTION__);
    return 0;
}


int  MediaPlayerHard::synchronize_audio(int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type() != AV_SYNC_AUDIO_MASTER) {
		av_log(NULL, AV_LOG_DEBUG,"%s: AV_SYNC_AUDIO_MASTER not eq AUDIO",__FUNCTION__);
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock(&this->audclk) - get_master_clock();

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            this->audio_diff_cum = diff + this->audio_diff_avg_coef * this->audio_diff_cum;
            if (this->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                this->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = this->audio_diff_cum * (1.0 - this->audio_diff_avg_coef);

                if (fabs(avg_diff) >= this->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * this->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = FFMIN(FFMAX(wanted_nb_samples, min_nb_samples), max_nb_samples);
                }
                av_dlog(NULL, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                        diff, avg_diff, wanted_nb_samples - nb_samples,
                        this->audio_clock, this->audio_diff_threshold);
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            this->audio_diff_avg_count = 0;
            this->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}

int MediaPlayerHard::audio_decode_frame()
{
    AVPacket *pkt_temp = &this->audio_pkt_temp;
    AVPacket *pkt = &this->audio_pkt;
    AVCodecContext *dec = this->audio_st->codec;
    int len1, data_size, resampled_data_size;
    int64_t dec_channel_layout;
    int got_frame;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    AVRational tb;
    int ret;
    int reconfigure;

    for (;;) {
        /* NOTE: the audio packet can contain several frames */
        while (pkt_temp->stream_index != -1 || this->audio_buf_frames_pending) {
            if (!this->frame) {
                if (!(this->frame = avcodec_alloc_frame())){
					av_log(NULL, AV_LOG_FATAL,"%s: avcodec_alloc_frame fail\n", __FUNCTION__);
                    return AVERROR(ENOMEM);
                }
            } else {
                av_frame_unref(this->frame);
                avcodec_get_frame_defaults(this->frame);
            }

            if (this->audioq.serial != this->audio_pkt_temp_serial){
                break;
            }

			if (!this->buffer_full){
				av_log(NULL, AV_LOG_DEBUG,"%s: buffer not full\n", __FUNCTION__);
				return -1;
			}

            if (this->paused){
				av_log(NULL, AV_LOG_DEBUG,"%s: in paused state\n", __FUNCTION__);
                return -1;
            }

            if (!this->audio_buf_frames_pending) {
				if( pkt_temp->data == this->flush_pkt.data){
					break;
				}
				if(pkt_temp->data ==NULL){
					break;
				}
                len1 = avcodec_decode_audio4(dec, this->frame, &got_frame, pkt_temp);
                if (len1 < 0) {
                    /* if error, we skip the frame */
                    pkt_temp->size = 0;
                    break;
                }

                pkt_temp->dts =
                pkt_temp->pts = AV_NOPTS_VALUE;
                pkt_temp->data += len1;
                pkt_temp->size -= len1;
                if (pkt_temp->data && pkt_temp->size <= 0 || !pkt_temp->data && !got_frame)
                    pkt_temp->stream_index = -1;
                if (!pkt_temp->data && !got_frame)
                    this->audio_finished = this->audio_pkt_temp_serial;

                if (!got_frame)
                    continue;

                tb = (AVRational){1, this->frame->sample_rate};
                if (this->frame->pts != AV_NOPTS_VALUE)
                    this->frame->pts = av_rescale_q(this->frame->pts, dec->time_base, tb);
                else if (this->frame->pkt_pts != AV_NOPTS_VALUE)
                    this->frame->pts = av_rescale_q(this->frame->pkt_pts, this->audio_st->time_base, tb);
                else if (this->audio_frame_next_pts != AV_NOPTS_VALUE)
                    this->frame->pts = av_rescale_q(this->audio_frame_next_pts, (AVRational){1, this->audio_src.freq}, tb);

                if (this->frame->pts != AV_NOPTS_VALUE)
                    this->audio_frame_next_pts = this->frame->pts + this->frame->nb_samples;
            }

            data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(this->frame),
                                                   this->frame->nb_samples,
                                                   (enum AVSampleFormat)this->frame->format, 1);

            dec_channel_layout =
                (this->frame->channel_layout && av_frame_get_channels(this->frame) == av_get_channel_layout_nb_channels(this->frame->channel_layout)) ?
                this->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(this->frame));
			///但由于是以声音为主，所以不做任何操作
            wanted_nb_samples = synchronize_audio(this->frame->nb_samples);
			///如果源声音配置和硬件声音配置不一致则需要转换
            if (this->frame->format        != this->audio_src.fmt            ||
                dec_channel_layout       != this->audio_src.channel_layout ||
                this->frame->sample_rate   != this->audio_src.freq           ||
                (wanted_nb_samples       != this->frame->nb_samples && !this->swr_ctx)) {
                swr_free(&this->swr_ctx);
                this->swr_ctx = swr_alloc_set_opts(NULL,
                                                 this->audio_tgt.channel_layout, this->audio_tgt.fmt, this->audio_tgt.freq,
                                                 dec_channel_layout,          (enum AVSampleFormat) this->frame->format, this->frame->sample_rate,
                                                 0, NULL);
                if (!this->swr_ctx || swr_init(this->swr_ctx) < 0) {
                    av_log(NULL, AV_LOG_ERROR,
                           "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                            this->frame->sample_rate, av_get_sample_fmt_name((enum AVSampleFormat)this->frame->format), av_frame_get_channels(this->frame),
                            this->audio_tgt.freq, av_get_sample_fmt_name(this->audio_tgt.fmt), this->audio_tgt.channels);
                    break;
                }
                this->audio_src.channel_layout = dec_channel_layout;
                this->audio_src.channels       = av_frame_get_channels(this->frame);
                this->audio_src.freq = this->frame->sample_rate;
                this->audio_src.fmt = (enum AVSampleFormat)this->frame->format;
            }

            if (this->swr_ctx) {
                const uint8_t **in = (const uint8_t **)this->frame->extended_data;
                uint8_t **out = &this->audio_buf1;
                int out_count = (int64_t)wanted_nb_samples * this->audio_tgt.freq / this->frame->sample_rate + 256;
                int out_size  = av_samples_get_buffer_size(NULL, this->audio_tgt.channels, out_count, this->audio_tgt.fmt, 0);
                int len2;
                if (out_size < 0) {
                    av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
                    break;
                }
                if (wanted_nb_samples != this->frame->nb_samples) {
                    if (swr_set_compensation(this->swr_ctx, (wanted_nb_samples - this->frame->nb_samples) * this->audio_tgt.freq / this->frame->sample_rate,
                                                wanted_nb_samples * this->audio_tgt.freq / this->frame->sample_rate) < 0) {
                        av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                        break;
                    }
                }
                av_fast_malloc(&this->audio_buf1, &this->audio_buf1_size, out_size);
                if (!this->audio_buf1)
                    return AVERROR(ENOMEM);
                len2 = swr_convert(this->swr_ctx, out, out_count, in, this->frame->nb_samples);
                if (len2 < 0) {
                    av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
                    break;
                }
                if (len2 == out_count) {
                    av_log(NULL, AV_LOG_DEBUG, "audio buffer is probably too small\n");
                    swr_init(this->swr_ctx);
                }
                this->audio_buf = this->audio_buf1;
                resampled_data_size = len2 * this->audio_tgt.channels * av_get_bytes_per_sample(this->audio_tgt.fmt);
            } else {
                this->audio_buf = this->frame->data[0];
                resampled_data_size = data_size;
            }

            audio_clock0 = this->audio_clock;
            /* update the audio clock with the pts */
            if (this->frame->pts != AV_NOPTS_VALUE){
                //this->audio_clock = this->frame->pts * av_q2d(tb) + (double) this->frame->nb_samples / this->frame->sample_rate;
                if( is_realtime(this->ic)){
					double tmp_clock = this->frame->pts * av_q2d(tb) + (double) this->frame->nb_samples / this->frame->sample_rate;
					if( tmp_clock >= this->audio_clock ){
						this->audio_clock_total = this->audio_clock_total + tmp_clock - this->audio_clock;
					}
					this->audio_clock = tmp_clock;
				}else{
					this->audio_clock = this->frame->pts * av_q2d(tb) + (double) this->frame->nb_samples / this->frame->sample_rate;
				}
            } else{
                this->audio_clock = NAN;
            }
            this->audio_clock_serial = this->audio_pkt_temp_serial;
#if 0
            {
                static double last_clock;
                av_log(NULL, AV_LOG_INFO,"%s:  audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n", //printf("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
					__FUNCTION__,
				 	is->audio_clock - last_clock,
                    is->audio_clock, audio_clock0);
                last_clock = is->audio_clock;
            }
#endif
            return resampled_data_size;
        } ///end while

        /* free the current packet */
        if (pkt->data){
            av_free_packet(pkt);
        }
        memset(pkt_temp, 0, sizeof(*pkt_temp));
        pkt_temp->stream_index = -1;

        if (this->audioq.abort_request) {
            return -1;
        }

        //if (is->audioq.nb_packets == 0)
        //    LYH_SignalCond(is->continue_read_thread);
		if (this->audioq.nb_packets <= 0 && !this->eof){
			//av_log(NULL, AV_LOG_DEBUG,"%s: is->audioq.nb_packets <=0 and not eof. set is->buffer_full=0\n", __FUNCTION__);
			if( this->buffer_full ){
				this->buffer_full = 0;
				notify(MEDIA_INFO, MEDIA_INFO_BUFFERING_START,0 , NULL);	
			}
            LYH_SignalCond(this->continue_read_thread);
			return -1;
		}

        /* read next packet */
		//av_log(NULL, AV_LOG_ERROR,"%s:  packet_queue_get into.\n", __FUNCTION__);
		ret = packet_queue_get(&this->audioq, pkt, 1, &this->audio_pkt_temp_serial);
		//av_log(NULL, AV_LOG_ERROR,"%s:  packet_queue_get out. ret=%d\n", __FUNCTION__,ret);
        if (ret < 0){
			av_log(NULL, AV_LOG_DEBUG,"%s: no audio packet\n", __FUNCTION__);
            return -1;
        }

        if (pkt->data == this->flush_pkt.data) {
            avcodec_flush_buffers(dec);
            this->audio_buf_frames_pending = 0;
            this->audio_frame_next_pts = AV_NOPTS_VALUE;
            if ((this->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !this->ic->iformat->read_seek)
                this->audio_frame_next_pts = this->audio_st->start_time;
        }

        *pkt_temp = *pkt;

		//由COMPLETED到STATED状态间隙，还会通知FIXME
		if(this->eof && pkt->data==NULL && !this->looping && !this->seek_req){
			av_log(NULL, AV_LOG_DEBUG,"%s: playback complete!\n", __FUNCTION__);
			notify(MEDIA_PLAYBACK_COMPLETE, 0, 0 , NULL);
		}
    }
}

///更新UI显示时间
/*
static void update_show_time(MediaPlayer *is,double pts)
{
	static long cur_time = 0;
	static int send_duration = 0;
	long tmp_time;

	//av_log(NULL, AV_LOG_INFO,"%s into,pts=%f\n", __FUNCTION__,pts);
	
	if(pts<0){
		tmp_time = 0;
	}
	tmp_time = pts;
	if(send_duration!=0 ){
		if(tmp_time==cur_time)
			return ;
		else
			cur_time = tmp_time;	
	}
	
	av_log(NULL, AV_LOG_INFO,"%s: cur_time=%ld\n",__FUNCTION__,cur_time);

	if(!is->jvm){
		av_log(NULL, AV_LOG_INFO,"%s is->jvm null\n", __FUNCTION__);
		return ;
	}

	// get the JNI env for this thread
  	bool isAttached = false;
  	JNIEnv* env = NULL;
  	if (is->jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
    	jint res = is->jvm->AttachCurrentThread(&env, NULL);
    	if ((res < 0) || !env) {
      		av_log(NULL, AV_LOG_INFO,
                   "%s: Could not attach thread to JVM (%d, %p)",
                   __FUNCTION__, res, env);
      		return ;
    	}
    	isAttached = true;
  	}
	
	if(is->time_update_cid==NULL){
		jclass splayer_class = env->FindClass("cn/com/sina/player/SPlayer");
		if (!splayer_class) {
			av_log(NULL, AV_LOG_INFO,"%s: could not find cn/com/sina/player/SPlayer", __FUNCTION__);
			return ;
		}

		is->time_update_cid = env->GetMethodID(splayer_class, "OnTimeUpdate", "(J)V");
		if (is->time_update_cid == NULL) {
			av_log(NULL, AV_LOG_INFO,"%s: could not get OnTimeUpdate ID", __FUNCTION__);
			return ;
		}
	}
	if(!send_duration){
		send_duration = 1;
		env->CallVoidMethod(is->splayer_obj, is->time_update_cid,(jlong)is->ic->duration);
	}else{
		env->CallVoidMethod(is->splayer_obj, is->time_update_cid,(jlong)cur_time);
	}

	if (isAttached) {
	  if (is->jvm->DetachCurrentThread() < 0) {
		av_log(NULL, AV_LOG_INFO,"%s: Could not detach thread from JVM",__FUNCTION__);
	  }
	}
}
*/

/* prepare a new audio buffer */
void MediaPlayerHard::sdl_audio_callback(void *opaque, char *stream, int len)
{
    MediaPlayerHard *is = (MediaPlayerHard *)opaque;
    int audio_size, len1;
    int bytes_per_sec;
    int frame_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, 1, is->audio_tgt.fmt, 1);

    int64_t audio_callback_time = av_gettime();

	//av_log(NULL, AV_LOG_DEBUG,"%s into\n", __FUNCTION__);

    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
           audio_size = is->audio_decode_frame();
           if (audio_size < 0) {
                /* if error, just output silence */
               is->audio_buf      = is->silence_buf;
               is->audio_buf_size = sizeof(is->silence_buf) / frame_size * frame_size;
           } else {
               is->audio_buf_size = audio_size;
           }
           is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        memcpy(stream, (char *)is->audio_buf + is->audio_buf_index, len1); ///memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    bytes_per_sec = is->audio_tgt.freq * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
	double tmp_pts=0;
    if (!isnan(is->audio_clock)) {
		///audio_size -> len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
		tmp_pts = is->audio_clock - (double)(1 * is->audio_hw_buf_size + is->audio_write_buf_size) / bytes_per_sec;
        is->set_clock_at(&is->audclk, tmp_pts, is->audio_clock_serial, audio_callback_time / 1000000.0); ///set_clock_at(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
        ////sync_clock_to_slave(&is->extclk, &is->audclk);
    }
	//av_log(NULL, AV_LOG_DEBUG,"%s out,is->audio_clock=%lf,tmp_pts=%lf,is->audio_write_buf_size=%d\n", __FUNCTION__, is->audio_clock, tmp_pts,is->audio_write_buf_size );
}

///返回缓冲区大小?
int MediaPlayerHard::audio_open(int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    LYH_AudioSpec wanted_spec, spec;
    const char *env;
    const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
	//MediaPlayer* is = (MediaPlayer*)opaque;

    av_log(NULL, AV_LOG_INFO,"%s into,wanted_channel_layout=%lld,wanted_nb_channels=%d,wanted_sample_rate=%d\n", 
		__FUNCTION__,wanted_channel_layout,wanted_nb_channels,wanted_sample_rate);
	
    env = "1"; 
    if (env) {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout); ///Number of channels: 1 mono, 2 stereo
    wanted_spec.freq = wanted_sample_rate; ///DSP frequency -- samples per second
    //if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
    //    av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!wanted_spec.channels=%d, wanted_spec.freq=%d\n",
	//		wanted_spec.channels, wanted_spec.freq);
    //    return -1;
    //}
    wanted_spec.format = LYH_AUDIO_S16;//AUDIO_S16SYS; ///Audio data format
    wanted_spec.silence = 0; ///Audio buffer silence value (calculated)
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE; ///Audio buffer size in samples (power of 2)
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = this;
    while (LYH_OpenAudio(this->hardware_context,&wanted_spec, &spec) < 0) {
        av_log(NULL, AV_LOG_WARNING, "LYH_OpenAudio (%d channels)\n", wanted_spec.channels);
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            av_log(NULL, AV_LOG_ERROR,"No more channel combinations to try, audio open failed\n");
            return -1;
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }
    if (spec.format != LYH_AUDIO_S16) { //AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio format %d is not supported!\n", spec.format);
        return -1;
    }
    //if (spec.channels != wanted_spec.channels) {
    //    wanted_channel_layout = av_get_default_channel_layout(spec.channels);
    //    if (!wanted_channel_layout) {
    //        av_log(NULL, AV_LOG_ERROR,
    //               "SDL advised channel count %d is not supported!\n", spec.channels);
    //        return -1;
    //    }
    //}
    wanted_channel_layout = av_get_default_channel_layout(spec.channels); ///MONO for android opensles

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels =  spec.channels;

	av_log(NULL, AV_LOG_INFO,"%s out\n", __FUNCTION__);
    return spec.size; ///Audio buffer size in bytes (calculated)
}

///move from cmd.cc
int MediaPlayerHard::check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
    int ret = avformat_match_stream_specifier(s, st, spec);
    if (ret < 0)
        av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
    return ret;
}

AVDictionary *MediaPlayerHard::filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,AVFormatContext *s, AVStream *st, AVCodec *codec)
{
    AVDictionary    *ret = NULL;
    AVDictionaryEntry *t = NULL;
    int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM
                                      : AV_OPT_FLAG_DECODING_PARAM;
    char          prefix = 0;
    const AVClass    *cc = avcodec_get_class();

    if (!codec)
        codec            = s->oformat ? avcodec_find_encoder(codec_id)
                                      : avcodec_find_decoder(codec_id);

    switch (st->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        prefix  = 'v';
        flags  |= AV_OPT_FLAG_VIDEO_PARAM;
        break;
    case AVMEDIA_TYPE_AUDIO:
        prefix  = 'a';
        flags  |= AV_OPT_FLAG_AUDIO_PARAM;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        prefix  = 's';
        flags  |= AV_OPT_FLAG_SUBTITLE_PARAM;
        break;
    }

    while (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) {
        char *p = strchr(t->key, ':');

        /* check stream specification in opt name */
        if (p)
            switch (check_stream_specifier(s, st, p + 1)) {
            case  1: *p = 0; break;
            case  0:         continue;
            default:         return NULL;
            }

        if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
            (codec && codec->priv_class &&
             av_opt_find(&codec->priv_class, t->key, NULL, flags,
                         AV_OPT_SEARCH_FAKE_OBJ)))
            av_dict_set(&ret, t->key, t->value, 0);
        else if (t->key[0] == prefix &&
                 av_opt_find(&cc, t->key + 1, NULL, flags,
                             AV_OPT_SEARCH_FAKE_OBJ))
            av_dict_set(&ret, t->key + 1, t->value, 0);

        if (p)
            *p = ':';
    }
    return ret;
}

//static AVDictionary *format_opts, *codec_opts, *resample_opts;
///end move
/* open a given stream. Return 0 if OK */
int MediaPlayerHard::stream_component_open(int stream_index)
{
    AVFormatContext *ic = this->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    const char *forced_codec_name = NULL;
    AVDictionary *opts;
    AVDictionaryEntry *t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret;
    int stream_lowres = this->lowres;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into,stream_index=%d\n",__FUNCTION__,stream_index);

	//new add
	if(this->abort_request){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL,"%s: return, abort_request set\n",__FUNCTION__);
		return -1;
	}

    if (stream_index < 0 || stream_index >= ic->nb_streams){
		av_log(NULL, AV_LOG_ERROR,"%s: stream index error %d\n",__FUNCTION__, stream_index);
        return -1;
    }
    avctx = ic->streams[stream_index]->codec;

	if(avctx->codec_type==AVMEDIA_TYPE_VIDEO){
		if(avctx->codec_id!=AV_CODEC_ID_H264){
			av_log(NULL, AV_LOG_ERROR, "%s: encoder(%d) not support\n",__FUNCTION__,avctx->codec_id);
			return -1;
		}else{
			av_log(NULL, AV_LOG_DEBUG, "%s: encoder(%d) support\n",__FUNCTION__,avctx->codec_id);
		}
		codec = avcodec_find_decoder_by_name("libstagefright_h264"); //codec = avcodec_find_decoder(avctx->codec_id);
		if (!codec) {				 
			av_log(NULL, AV_LOG_ERROR,"No codec could be found with id %d\n", avctx->codec_id);
			return -1;
		}
		///av_log(NULL, AV_LOG_INFO, "%s: avcodec_find_decoder end\n",__FUNCTION__);
	}else{
		codec = avcodec_find_decoder(avctx->codec_id);
		if (!codec) {				 
			av_log(NULL, AV_LOG_ERROR,"No codec could be found with id %d\n", avctx->codec_id);
			return -1;
		}
	}
	
    switch(avctx->codec_type){
        case AVMEDIA_TYPE_AUDIO   : 
			this->last_audio_stream = stream_index; 
			//forced_codec_name = audio_codec_name; 
			break;
        case AVMEDIA_TYPE_SUBTITLE: 
			this->last_subtitle_stream = stream_index; 
			//forced_codec_name = subtitle_codec_name; 
			break;
        case AVMEDIA_TYPE_VIDEO   : 
			this->last_video_stream = stream_index; 
			//forced_codec_name = video_codec_name; 
			break;
    }
    //if (forced_codec_name){
    //    codec = avcodec_find_decoder_by_name(forced_codec_name);
    //}
    //if (!codec) {
        //if (forced_codec_name) 
		//	av_log(NULL, AV_LOG_WARNING,"No codec could be found with name '%s'\n", forced_codec_name);
        //else                   
		//	av_log(NULL, AV_LOG_WARNING,"No codec could be found with id %d\n", avctx->codec_id);
        //return -1;
    //}

	///av_log(NULL, AV_LOG_INFO, "%s: step 1\n",__FUNCTION__);

    avctx->codec_id = codec->id;
    avctx->workaround_bugs   = 1; //workaround_bugs;
    if(stream_lowres > av_codec_get_max_lowres(codec)){
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
                av_codec_get_max_lowres(codec));
        stream_lowres = av_codec_get_max_lowres(codec);
    }
    av_codec_set_lowres(avctx, stream_lowres);
    avctx->error_concealment = 3; //error_concealment;  0b00000011 both

    if(stream_lowres){
		avctx->flags |= CODEC_FLAG_EMU_EDGE;
    }
    if (this->fast){   
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: fast=%d\n", __FUNCTION__,this->fast);
		avctx->flags2 |= CODEC_FLAG2_FAST;
    }
    if(codec->capabilities & CODEC_CAP_DR1){
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
    }
	///new add
	///skip frame
	
	if(AVMEDIA_TYPE_VIDEO==avctx->codec_type){
		if( this->skip_frame >= 4 ) avctx->skip_frame = AVDISCARD_ALL;
		else if( this->skip_frame == 3 ) avctx->skip_frame = AVDISCARD_NONKEY;
		else if( this->skip_frame == 2 ) avctx->skip_frame = AVDISCARD_BIDIR;
		else if( this->skip_frame == 1 ) avctx->skip_frame = AVDISCARD_NONREF;
		else if( this->skip_frame == 0 ) avctx->skip_frame = AVDISCARD_DEFAULT;
		else if( this->skip_frame == -1 ) avctx->skip_frame = AVDISCARD_NONE;
		else avctx->skip_frame = AVDISCARD_DEFAULT;
		///skip idct
		if( this->skip_idct >= 4 ) avctx->skip_idct = AVDISCARD_ALL;
		else if( this->skip_idct == 3 ) avctx->skip_idct = AVDISCARD_NONKEY;
		else if( this->skip_idct == 2 ) avctx->skip_idct = AVDISCARD_BIDIR;
		else if( this->skip_idct == 1 ) avctx->skip_idct = AVDISCARD_NONREF;
		else if( this->skip_idct == 0 ) avctx->skip_idct = AVDISCARD_DEFAULT;
		else if( this->skip_idct == -1 ) avctx->skip_idct = AVDISCARD_NONE;
		else avctx->skip_idct = AVDISCARD_DEFAULT;
		///skip loopfilter
		if( this->skip_loopfilter>= 4 ) avctx->skip_loop_filter= AVDISCARD_ALL;
		else if( this->skip_loopfilter == 3 ) avctx->skip_loop_filter = AVDISCARD_NONKEY;
		else if( this->skip_loopfilter == 2 ) avctx->skip_loop_filter = AVDISCARD_BIDIR;
		else if( this->skip_loopfilter == 1 ) avctx->skip_loop_filter = AVDISCARD_NONREF;
		else if( this->skip_loopfilter == 0 ) avctx->skip_loop_filter = AVDISCARD_DEFAULT;
		else if( this->skip_loopfilter == -1 ) avctx->skip_loop_filter = AVDISCARD_NONE;
		else avctx->skip_loop_filter = AVDISCARD_DEFAULT;
		av_log(NULL, AV_LOG_INFO, "%s: avctx: skip frame=%d, skip idct=%d,skip loopfilter=%d,lowres=%d\n", __FUNCTION__,avctx->skip_frame,avctx->skip_idct,avctx->skip_loop_filter,avctx->lowres);
	}
	
	///end
	
	///av_log(NULL, AV_LOG_INFO, "%s: step 2\n",__FUNCTION__);
	
	///maybe dead for this reason
    opts = filter_codec_opts(codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec);
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
	
    if (stream_lowres)
        av_dict_set(&opts, "lowres", av_asprintf("%d", stream_lowres), AV_DICT_DONT_STRDUP_VAL);

	if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0);

	ret = avcodec_open2(avctx, codec, &opts);
    if (ret < 0){ ///打开解码器
    	av_log(NULL, AV_LOG_ERROR, "%s: avcodec_open2 fail,%d\n", __FUNCTION__,ret);
        return -1;
    }
	
	if(AVMEDIA_TYPE_VIDEO==avctx->codec_type){
		//start decoder
		send_decoder_cmd(avctx,ANDROID_SET_JVM_CMD,(void*)this->jvm);
		send_decoder_cmd(avctx,ANDROID_SET_MCLASS_CMD,(void*)this->surface_render_listener->getMediaCodecClass());
		send_decoder_cmd(avctx,ANDROID_SET_SURFACE_CMD,(void*)this->surface_render_listener->getSurface());
		if( send_decoder_cmd(avctx,ANDROID_DECODER_START_CMD,NULL)<0 ){
			av_log(NULL, AV_LOG_ERROR, "%s: avcodec start mediacodec fail\n", __FUNCTION__);
			///should info mediaplayer listener
			this->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_DECODER_FAIL , NULL);
			return -1;
		}
	}
	
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

	///av_log(NULL, AV_LOG_INFO, "%s: step 3\n",__FUNCTION__);
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: openning audio stream\n",__FUNCTION__);
        sample_rate    = avctx->sample_rate;
        nb_channels    = avctx->channels;
        channel_layout = avctx->channel_layout;

        /* prepare audio output */ //准备声音输出
        if ((ret = audio_open(channel_layout, nb_channels, sample_rate, &this->audio_tgt)) < 0){
			av_log(NULL, AV_LOG_ERROR, "%s: open audio device fail\n",__FUNCTION__);
			return ret;
        }
        this->audio_hw_buf_size = ret;
        this->audio_src = this->audio_tgt;
        this->audio_buf_size  = 0;
        this->audio_buf_index = 0;

        /* init averaging filter */
        this->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        this->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio fifo fullness,
           we correct audio sync only if larger than this threshold */
        this->audio_diff_threshold = 2.0 * this->audio_hw_buf_size / av_samples_get_buffer_size(NULL, this->audio_tgt.channels, this->audio_tgt.freq, this->audio_tgt.fmt, 1);

        memset(&this->audio_pkt, 0, sizeof(this->audio_pkt));
        memset(&this->audio_pkt_temp, 0, sizeof(this->audio_pkt_temp));
        this->audio_pkt_temp.stream_index = -1;

        this->audio_stream = stream_index;
        this->audio_st = ic->streams[stream_index];

        packet_queue_start(&this->audioq);
		///////////开启线程
        ///LYH_PauseAudio(is->hardware_context,1);  
        break;
    case AVMEDIA_TYPE_VIDEO:
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: openning video stream\n",__FUNCTION__);
        this->video_stream = stream_index;
        this->video_st = ic->streams[stream_index];
		
        packet_queue_start(&this->videoq);
		///////////开启线程
        ///is->video_tid = LYH_CreateThread(video_thread, is);
        this->queue_attachments_req = 1;

		this->frame_width = avctx->width;
		this->frame_height = avctx->height;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: openning subtitle stream\n",__FUNCTION__);
        this->subtitle_stream = stream_index;
        this->subtitle_st = ic->streams[stream_index];
	
        packet_queue_start(&this->subtitleq);
		///////////开启线程
        ///is->subtitle_tid = LYH_CreateThread(subtitle_thread, is);
        break;
    default:
        break;
    }

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
    return 0;
}

void MediaPlayerHard::stream_component_close(int stream_index)
{
    AVFormatContext *ic = this->ic;
    AVCodecContext *avctx;

	av_log(NULL, AV_LOG_DEBUG, "%s: into,stream_index=%d\n",__FUNCTION__,stream_index);
	
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: closing audio stream\n",__FUNCTION__);
        packet_queue_abort(&this->audioq);

        LYH_CloseAudio(this->hardware_context); 

        packet_queue_flush(&this->audioq);
        av_free_packet(&this->audio_pkt);
        swr_free(&this->swr_ctx);
		this->swr_ctx = NULL;
        av_freep(&this->audio_buf1);
		this->audio_buf1 = NULL;
        this->audio_buf1_size = 0;
        this->audio_buf = NULL;
        av_frame_free(&this->frame);
		this->frame = NULL;
		//av_log(NULL, AV_LOG_ERROR, "%s: closing audio stream end\n",__FUNCTION__);
        break;
    case AVMEDIA_TYPE_VIDEO:
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: closing video stream\n",__FUNCTION__);
        packet_queue_abort(&this->videoq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        LYH_LockMutex(this->pictq_mutex); 
        LYH_SignalCond(this->pictq_cond); 
        LYH_UnlockMutex(this->pictq_mutex); 

        LYH_WaitThread(this->video_tid, NULL);
		this->video_tid = 0; //add

        packet_queue_flush(&this->videoq);
		if(NULL!=this->video_decode_frame){
			av_frame_free(&this->video_decode_frame);
			this->video_decode_frame = NULL;
		}
		//av_log(NULL, AV_LOG_ERROR, "%s: close video stream end\n",__FUNCTION__);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        packet_queue_abort(&this->subtitleq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        LYH_LockMutex(this->subpq_mutex); 
        LYH_SignalCond(this->subpq_cond); 
        LYH_UnlockMutex(this->subpq_mutex); 

        LYH_WaitThread(this->subtitle_tid, NULL); 
		this->subtitle_tid = 0;

        packet_queue_flush(&this->subtitleq);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
	av_log(NULL, AV_LOG_INFO, "%s: closing %d decodec\n",__FUNCTION__,avctx->codec_type);
    avcodec_close(avctx);
	av_log(NULL, AV_LOG_INFO, "%s: closed %d decodec\n",__FUNCTION__,avctx->codec_type);
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        this->audio_st = NULL;
        this->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        this->video_st = NULL;
        this->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        this->subtitle_st = NULL;
        this->subtitle_stream = -1;
        break;
    default:
        break;
    }

	av_log(NULL, AV_LOG_DEBUG, "%s: out,stream_index=%d\n",__FUNCTION__,stream_index);
}


int MediaPlayerHard::decode_interrupt_cb(void *ctx)
{
	av_log(NULL, AV_LOG_INFO, "%s: into\n",__FUNCTION__);

    MediaPlayerHard *is = (MediaPlayerHard *)ctx;
    return is->abort_request;
}

int MediaPlayerHard::is_realtime(AVFormatContext *s)
{
	if(!s){
		return 0;
	}
	if( !strncmp(s->iformat->name, "hls", 3) ){
			return 1;
	}

    if(   !strcmp(s->iformat->name, "rtp")|| 
		  !strcmp(s->iformat->name, "rtsp")|| 
		  !strcmp(s->iformat->name, "sdp")
    ){
        return 1;
    }

    if(s->pb && (   !strncmp(s->filename, "rtp:", 4)
                 || !strncmp(s->filename, "udp:", 4)
                )
    ){
        return 1;
    }
	
    return 0;
}


int MediaPlayerHard::is_seekable(AVFormatContext *s)
{
	if(NULL!=s && NULL!=s->pb){
		return s->pb->seekable;
	}
	return 0;
}


///move from cmd.cc
AVDictionary **MediaPlayerHard::setup_find_stream_info_opts(AVFormatContext *s,AVDictionary *codec_opts)
{
    int i;
    AVDictionary **opts;
	
    if (!s->nb_streams)
        return NULL;
    opts = (AVDictionary**)av_mallocz(s->nb_streams * sizeof(*opts));
    if (!opts) {
        av_log(NULL, AV_LOG_ERROR,
               "Could not alloc memory for stream options.\n");
        return NULL;
    }
    for (i = 0; i < s->nb_streams; i++)
        opts[i] = filter_codec_opts(codec_opts, s->streams[i]->codec->codec_id,
                                    s, s->streams[i], NULL);
    return opts;
}
///
/* this thread gets the stream from the disk or the network */
void* MediaPlayerHard::read_thread(void *arg)
{
	//int eof = 0;
	int err, i, ret;
	AVPacket pkt1, *pkt = &pkt1;
	LYH_Mutex *wait_mutex = LYH_CreateMutex();
	MediaPlayerHard *is = (MediaPlayerHard *)arg;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: read thread into\n",__FUNCTION__);

	if(!is->buffer_full){
		is->notify(MEDIA_INFO, MEDIA_INFO_BUFFERING_START,0 , NULL);
	}

	//av_log(NULL, AV_LOG_ERROR, "%s: is->ic->iformat->name=%s\n",__FUNCTION__,is->ic->iformat->name); 
    for (;;) {
        if (is->abort_request){
			av_log(NULL, AV_LOG_DEBUG, "%s: is->abort_request true\n",__FUNCTION__);
            break;
        }
        /*if (is->paused != is->last_paused) {
            		is->last_paused = is->paused;
            		if (is->paused)
                		is->read_pause_return = av_read_pause(ic);
            		else
                		av_read_play(ic);
        	}*/
        	
        if (is->seek_req) {
			av_log(NULL, AV_LOG_DEBUG,"%s: recv a seek req\n", __FUNCTION__);
			if(is->is_seekable(is->ic)){
	            int64_t seek_target = is->seek_pos;
	            int64_t seek_min    = is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
	            int64_t seek_max    = is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
				// FIXME the +-2 is due to rounding being not done in the correct direction in generation
				//      of the seek_pos/seek_rel variables
	            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
	            if (ret < 0) {
	                av_log(NULL, AV_LOG_ERROR,"%s: error while seeking\n", is->ic->filename);
	            } else {
	            	/*if(is->send_seek_complete_notify){
	            				play_notify_hard(is,MEDIA_SEEK_COMPLETE, 0,0 , NULL);
						is->send_seek_complete_notify = 0;
	            			}*/
	            	is->buffer_full = 0;
					
	                if (is->audio_stream >= 0) {
	                    is->packet_queue_flush(&is->audioq);
	                    is->packet_queue_put(&is->audioq, &is->flush_pkt);
	                }
	                if (is->subtitle_stream >= 0) {
	                    is->packet_queue_flush(&is->subtitleq);
	                    is->packet_queue_put(&is->subtitleq, &is->flush_pkt);
	                }
	                if (is->video_stream >= 0) {
	                    is->packet_queue_flush(&is->videoq);
	                    is->packet_queue_put(&is->videoq, &is->flush_pkt);
	                }
					
					is->notify(MEDIA_INFO, MEDIA_INFO_BUFFERING_START,0 , NULL);
	            }
	            is->seek_req = 0;
	            is->queue_attachments_req = 1;
	            is->eof = 0;
				/*
	            		if (is->paused){ ///如果播放暂停则需要播放一帧再暂停
	                		step_to_next_frame(is);
	            		}else{
					if(is->send_seek_complete_notify){
	            				play_notify_hard(is,MEDIA_SEEK_COMPLETE, 0,0 , NULL);
						is->send_seek_complete_notify = 0;
	            			}
					is->seeking = 0; 
				} */
				//if (is->paused){
	                is->stream_toggle_pause(1); //step_to_next_frame(is);
	                if( is->state == SPALYER_STATE_PAUSED ){
						is->state = SPALYER_STATE_STARTED;
					}
	            //}
				if(is->send_seek_complete_notify){
					//更新声音时间，不用更新声音同步clock
	            	is->notify(MEDIA_SEEK_COMPLETE, 0,0 , NULL);
					is->send_seek_complete_notify = 0;
	            }
				//应该显示seekto位置时间，更新声音时间，不用更新声音同步clock
				if(is->is_realtime(is->ic)){
					is->audio_clock_total = is->seek_pos / 1000000;
				}else{
					is->audio_clock = is->seek_pos / 1000000;
				}
				
				is->seeking = 0; 
			}else{
				//av_log(NULL, AV_LOG_DEBUG,"%s: hls living not support seeking\n",__FUNCTION__);
				is->seek_req = 0;
				//if (is->paused){
	                is->stream_toggle_pause(1); //step_to_next_frame(is);
	                if( is->state == SPALYER_STATE_PAUSED ){
						is->state = SPALYER_STATE_STARTED;
					}
	            //}
				if(is->send_seek_complete_notify){
	            	is->notify(MEDIA_SEEK_COMPLETE, 0,0 , NULL);
					is->send_seek_complete_notify = 0;
	            }
				is->seeking = 0;
			}
        }
		
        if (is->queue_attachments_req) {			
			av_log(NULL, AV_LOG_INFO, "%s: recv queue_attachments_req\n", __FUNCTION__);            
			if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
				AVPacket copy;                
				if ((ret = av_copy_packet(&copy, &is->video_st->attached_pic)) < 0)                    
					goto fail;
				av_log(NULL, AV_LOG_INFO, "%s: put a attached_pic into videoq\n", __FUNCTION__);
				is->packet_queue_put(&is->videoq, &copy);                
				is->packet_queue_put_nullpacket(&is->videoq, is->video_stream);            
			}            
			is->queue_attachments_req = 0;        
		}

        // if the queue are full, no need to read more 
        if ( ( is->eof || 
              ( (is->audioq.nb_packets > is->MAX_FRAMES || is->audio_stream < 0 || is->audioq.abort_request) && 
                (is->videoq.nb_packets > is->MAX_FRAMES || is->video_stream < 0 || is->videoq.abort_request || (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) ) ) ) {
                //(is->subtitleq.nb_packets > is->MAX_FRAMES || is->subtitle_stream < 0 || is->subtitleq.abort_request) ) ) ) {
            // wait 10 ms
            if(!is->buffer_full){
				if(is->eof){
					av_log(NULL, AV_LOG_DEBUG, "%s: EOF ,set buffer full\n", __FUNCTION__);
				}
				is->buffer_full = 1;
				is->notify(MEDIA_INFO, MEDIA_INFO_BUFFERING_END,0 , NULL);
				av_log(NULL, AV_LOG_DEBUG, "%s: buffer full,is->audioq.nb_packets=%d,is->videoq.nb_packets=%d\n",__FUNCTION__,is->audioq.nb_packets, is->videoq.nb_packets);
            }
			if(!is->eof){
	            LYH_LockMutex(wait_mutex); 
	            LYH_TimeWaitCond(is->continue_read_thread, wait_mutex, 10);
	            LYH_UnlockMutex(wait_mutex);
	            continue;
			}
        }
		
		if(!is->eof && !is->buffer_full){
			//av_log(NULL, AV_LOG_DEBUG, "%s: buffering... is->audioq.size=%d,is->videoq.size=%d,is->subtitleq.size=%d,is->audioq.nb_packets=%d,is->videoq.nb_packets=%d,is->subtitleq.nb_packets=%d,is->audioq.abort_request=%d,is->videoq.abort_request=%d,is->eof=%d\n", 
			//				__FUNCTION__,is->audioq.size,is->videoq.size,is->subtitleq.size,is->audioq.nb_packets,is->videoq.nb_packets,is->subtitleq.nb_packets,is->audioq.abort_request,is->videoq.abort_request,is->eof);
			int min_packets_frames = FFMIN(is->audioq.nb_packets,is->videoq.nb_packets);
			float tmp_max_frames = is->MAX_FRAMES;
			float tmp_min_packets_frames = min_packets_frames;
			float percent =  (tmp_min_packets_frames/tmp_max_frames)*100;
			if(is->laset_buffer_percent!=(int)percent){
				is->laset_buffer_percent = percent;
				is->notify(MEDIA_BUFFERING_UPDATE, (int)percent, 0, NULL);
				//av_log(NULL, AV_LOG_DEBUG, "%s: MEDIA_BUFFERING_UPDATE percent=%d\n", __FUNCTION__,is->laset_buffer_percent);
			}
			
		}
        if (!is->paused &&
            (!is->audio_st || is->audio_finished == is->audioq.serial) &&
            (!is->video_st || (is->video_finished == is->videoq.serial && is->pictq_size == 0))) {
            av_log(NULL, AV_LOG_DEBUG, "%s: read at the end of file, is->looping=%d\n", __FUNCTION__, is->looping);
           
            if ( is->looping ) { 
                is->stream_seek(0, 0, 0); 
            } 
        }
        if (is->eof) {
            if (is->video_stream >= 0){
                is->packet_queue_put_nullpacket(&is->videoq, is->video_stream);
            }
            if (is->audio_stream >= 0){
                is->packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
            }
            LYH_Delay(10);
            //eof=0;
            continue;
        }
		//av_log(NULL, AV_LOG_ERROR, "%s: av_read_frame into\n",__FUNCTION__);
        ret = av_read_frame(is->ic, pkt); ///0 OK or <0 error happen
		//av_log(NULL, AV_LOG_ERROR, "%s: av_read_frame out ret=%d\n",__FUNCTION__,ret);
        if (ret < 0) {
			//if(is->ic->pb){
			//	av_log(NULL, AV_LOG_ERROR, "%s: is->ic->pb->error=%d\n",__FUNCTION__,is->ic->pb->error);
			//}
			//if(ret == AVERROR_EOF){
			//	av_log(NULL, AV_LOG_ERROR, "%s: ret == AVERROR_EOF\n",__FUNCTION__);
			//}
            if (ret == AVERROR_EOF || url_feof(is->ic->pb)){
				if(ret == AVERROR_EOF){
					av_log(NULL, AV_LOG_DEBUG, "%s: ret == AVERROR_EOF\n",__FUNCTION__);
					//if(strncmp(is->ic->iformat->name,"hls",3)){
						/*
						int dmsec = is->duration/1000 ;
						int cmsec = is->audio_clock* 1000;
						int smsec = is->seek_pos/1000;
						av_log(NULL, AV_LOG_WARNING, "%s: not m3u8 live,dmsec=%d,cmsec=%d,smsec=%d\n",__FUNCTION__,dmsec,cmsec,smsec);
						if(cmsec==0 && is->seek_pos>0){
							av_log(NULL, AV_LOG_WARNING, "%s: first seek to end, smsec=%d,dmsec=%d\n",__FUNCTION__,smsec,dmsec);
							is->eof = 1;
						}else if(is->seek_pos>0 && smsec+2000>=dmsec ){
							av_log(NULL, AV_LOG_WARNING, "%s: last seek to end, smsec=%d,dmsec=%d\n",__FUNCTION__,smsec,dmsec);
							is->eof = 1;
						}else if(cmsec+2000 < dmsec){
							av_log(NULL, AV_LOG_WARNING, "%s: not reach play end yet,cmsec=%d,dmsec=%d\n",__FUNCTION__,cmsec,dmsec);
							////////////原来是没的，用于断网时报EOF，断网后网好了还要继续播放
							is->eof = 1;  ///暂时忽略断网要求
						}else{
							av_log(NULL, AV_LOG_WARNING, "%s: reach play end ,cmsec=%d,dmsec=%d\n",__FUNCTION__,cmsec,dmsec);
							is->eof = 1;
						}
						*/
						if(is->ic->pb && is->ic->pb->error<0){
							av_log(NULL, AV_LOG_DEBUG, "%s: is->ic->pb->error happen, not normal AVERROR_EOF\n",__FUNCTION__);
						}else{
							av_log(NULL, AV_LOG_DEBUG, "%s: normal AVERROR_EOF, set EOF\n",__FUNCTION__);
							is->eof = 1;
						}
					//}
				}else {
					av_log(NULL, AV_LOG_DEBUG, "%s: ret != AVERROR_EOF\n",__FUNCTION__);
				}
				//av_log(NULL, AV_LOG_WARNING, "%s: reach file end\n",__FUNCTION__);
                ///is->eof = 1;
            }else{
				av_log(NULL, AV_LOG_DEBUG, "%s: others read error,ret=%d\n",__FUNCTION__,ret);
			}
            //if (is->ic->pb && is->ic->pb->error){
				///测试无网或者读错误时还能继续
				///play_notify_hard(is,MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_IO, NULL);
				///av_log(NULL, AV_LOG_ERROR, "%s: IO Error=%d happen, exit read!\n",__FUNCTION__,is->ic->pb->error);
                ///break;
                ///play_notify_hard(is,MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_IO, NULL);
			//	av_log(NULL, AV_LOG_WARNING, "%s: IO Error=%d happen!\n",__FUNCTION__,is->ic->pb->error);
            //}
            LYH_LockMutex(wait_mutex);
            LYH_TimeWaitCond(is->continue_read_thread, wait_mutex, 10);
            LYH_UnlockMutex(wait_mutex);
            continue;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        //stream_start_time = ic->streams[pkt->stream_index]->start_time;
        //pkt_in_play_range = duration == AV_NOPTS_VALUE ||
        //        (pkt->pts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
        //        av_q2d(ic->streams[pkt->stream_index]->time_base) -
        //        (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
        //        <= ((double)duration / 1000000);
        //av_log(NULL, AV_LOG_ERROR, "%s: packet_queue_put into\n",__FUNCTION__);
        if (pkt->stream_index == is->audio_stream ){ //&& pkt_in_play_range) {
            is->packet_queue_put(&is->audioq, pkt);
        } else if (pkt->stream_index == is->video_stream //&& pkt_in_play_range
                   && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            is->packet_queue_put(&is->videoq, pkt);
        } else if (pkt->stream_index == is->subtitle_stream){ //&& pkt_in_play_range) {
            is->packet_queue_put(&is->subtitleq, pkt);
        } else {
            av_free_packet(pkt);
        }
		//av_log(NULL, AV_LOG_ERROR, "%s: packet_queue_put out\n",__FUNCTION__);
    }
    /* wait until the end */
    while (!is->abort_request) {
		av_log(NULL, AV_LOG_DEBUG, "%s: loop here\n",__FUNCTION__);
        LYH_Delay(100);
    }

    ret = 0;
 fail:
    /* close each stream */
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: close stream begin\n",__FUNCTION__);
    if (is->audio_stream >= 0)
        is->stream_component_close(is->audio_stream);
    if (is->video_stream >= 0)
        is->stream_component_close(is->video_stream);
    if (is->subtitle_stream >= 0)
        is->stream_component_close(is->subtitle_stream);
    if (is->ic) {
        avformat_close_input(&is->ic);
    }
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: close stream end\n",__FUNCTION__);
	
    if (ret != 0) {
        LYH_event event; 

        event.type = LYH_QUIT_EVENT;
        event.data = is; 
        LYH_PushEvent(is->hardware_context,&event);
    }
    LYH_DestroyMutex(wait_mutex);

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: read thread exit\n",__FUNCTION__);
    return 0;
}

int MediaPlayerHard::videostate_init(AVInputFormat *iformat)
{
    av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	av_init_packet(&this->flush_pkt);
    this->flush_pkt.data = (uint8_t *)&this->flush_pkt;

	//is->filename = (char*)av_mallocz( strlen(is->filename)+1);
	//strcpy(is->filename, filename);
	
    this->iformat = iformat;
    this->ytop    = 0;
    this->xleft   = 0;

    /* start video display */
    this->pictq_mutex = LYH_CreateMutex(); 
    this->pictq_cond  = LYH_CreateCond();

    this->subpq_mutex = LYH_CreateMutex();
    this->subpq_cond  = LYH_CreateCond();

    packet_queue_init(&this->videoq);
    packet_queue_init(&this->audioq);
    packet_queue_init(&this->subtitleq);

    this->continue_read_thread = LYH_CreateCond();

	this->buffer_full = 0;

	this->loop = 1 ; //never use

	//config
	//is->fast = 0;  //default is 0
	//is->lowres = 0;  //default is 0
	if(0==this->framedrop)
		this->framedrop = -1;
	if(0==this->MAX_FRAMES)
		this->MAX_FRAMES = 125;
	if(0==this->MAX_QUEUE_SIZE){
		this->MAX_QUEUE_SIZE = 250*1024;
	}
	//this->skip_frame = 0;  //default is 0
	//this->skip_idct = 0;  //default is 0
	//this->skip_loopfilter = 0;  //default is 0

	
	this->wanted_stream[AVMEDIA_TYPE_AUDIO] = -1;
	this->wanted_stream[AVMEDIA_TYPE_VIDEO] = -1;
	this->wanted_stream[AVMEDIA_TYPE_SUBTITLE] = -1;
	
	
    init_clock(&this->vidclk, &this->videoq.serial);
    init_clock(&this->audclk, &this->audioq.serial);
    ////init_clock(&is->extclk, &is->extclk.serial);
    this->audio_clock_serial = -1;
    this->audio_last_serial = -1;
    this->av_sync_type = AV_SYNC_AUDIO_MASTER; //av_sync_type;
	//av_log(NULL, AV_LOG_INFO, "%s: LYH_CreateThread begin\n",__FUNCTION__);

	///java callback
	//is->jvm = jvm;
	if(this->hardware_context==NULL){
		this->hardware_context = new LYH_Context();
		this->hardware_context->lyh_event_queue_mtx = NULL;
		this->hardware_context->audio_device_android_opensles = NULL;
	}

	this->mp_listener_lock = LYH_CreateMutex();

	this->eof = 0;

	//this->event_thread_id = -1;
	//this->stream_open_thread_id = -1;

	this->last_notify_msg = -1;
	this->send_seek_complete_notify = 0;

	this->frame_drops_early_continue = 0;
	this->frame_drops_late_continue = 0;
	this->frame_drop_step = 0;

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);
	
	return 0;
	
}

int MediaPlayerHard::do_stream_open( )
{
	AVFormatContext *ic = NULL;
   	int err, i, ret = 0;
   	int st_index[AVMEDIA_TYPE_NB];
    AVDictionaryEntry *t;
    AVDictionary **opts;
    int orig_nb_streams;
    

   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

   	memset(st_index, -1, sizeof(st_index));
	this->last_video_stream = this->video_stream = -1;
	this->last_audio_stream = this->audio_stream = -1;
	this->last_subtitle_stream = this->subtitle_stream = -1;

	ic = avformat_alloc_context();
	ic->interrupt_callback.callback = decode_interrupt_cb;
	ic->interrupt_callback.opaque = this;
	//av_log(NULL, AV_LOG_INFO, "%s: avformat_open_input begin,is->filename=%s\n",__FUNCTION__,this->filename);
	err = avformat_open_input(&ic, this->filename, this->iformat, &format_opts);
	if (err < 0) {
		av_log(NULL, AV_LOG_ERROR, "%s: avformat_open_input fail,err=%d\n",__FUNCTION__,err);
		ret = -1;
		goto fail;
	}
   	if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
	   av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
	   ret = AVERROR_OPTION_NOT_FOUND;
	   goto fail;
   	}
   	this->ic = ic;
	this->duration = this->ic->duration;
   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: avformat_open_input end,ic->duration=%lld,IO context seekable=%d\n",
		__FUNCTION__,this->ic->duration, this->ic->pb->seekable);
	
   	if (this->genpts)
	   ic->flags |= AVFMT_FLAG_GENPTS;
   	opts = setup_find_stream_info_opts(ic, codec_opts);
   	orig_nb_streams = ic->nb_streams;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: avformat_find_stream_info into\n",__FUNCTION__);
   	err = avformat_find_stream_info(ic, opts);  ///modify by lyhh for compile error
   	if (err < 0) {
	   av_log(NULL, AV_LOG_WARNING,
			  "%s: could not find codec parameters\n", this->filename);
	   ret = -1;
	   goto fail;
   	}
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: avformat_find_stream_info out\n",__FUNCTION__);
   	for (i = 0; i < orig_nb_streams; i++)
	   av_dict_free(&opts[i]);
   	av_freep(&opts); 

   	if (ic->pb)
	   ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use url_feof() to test for the end

	//if(!strncmp(is->ic->iformat->name,"hls",3)){
	//	is->max_frame_duration = 10.0;
	//}else{
	//	is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
	//}
   	this->max_frame_duration = 10.0;
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: is->max_frame_duration=%lf\n",__FUNCTION__,this->max_frame_duration);


   	this->realtime = is_realtime(ic);
   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: av_find_best_stream begin\n",__FUNCTION__);
   	for (i = 0; i < ic->nb_streams; i++)
	   ic->streams[i]->discard = AVDISCARD_ALL;
   	if (!this->video_disable){
	   st_index[AVMEDIA_TYPE_VIDEO] =
		   av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
							   this->wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
	   av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: av_find_best_stream  video end\n",__FUNCTION__);
   	}
   	if (!this->audio_disable){
	   st_index[AVMEDIA_TYPE_AUDIO] =
		   av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
							   this->wanted_stream[AVMEDIA_TYPE_AUDIO],
							   st_index[AVMEDIA_TYPE_VIDEO],
							   NULL, 0);
	   av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: av_find_best_stream  audio end\n",__FUNCTION__);
   	}
   	if (!this->video_disable && !this->subtitle_disable){
	   st_index[AVMEDIA_TYPE_SUBTITLE] =
		   av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
							   this->wanted_stream[AVMEDIA_TYPE_SUBTITLE],
							   (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
								st_index[AVMEDIA_TYPE_AUDIO] :
								st_index[AVMEDIA_TYPE_VIDEO]),
							   NULL, 0);
	   av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: av_find_best_stream subtitle end\n",__FUNCTION__);
   	}
	
   	if (0) { 
	   av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: av_dump_format begin\n",__FUNCTION__);
	   av_dump_format(ic, 0, this->filename, 0);
	   av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: av_dump_format end\n",__FUNCTION__);
   	}

   	this->show_mode = SHOW_MODE_NONE; //show_mode;

   	if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
	   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_component_open audio begin\n",__FUNCTION__);
	   	ret = stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
	   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_component_open audio end\n",__FUNCTION__);
	   	if(ret<0){
			av_log(NULL, AV_LOG_ERROR, "%s: stream_component_open audio fail\n",__FUNCTION__);
			goto fail;
		}
   	}

   	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
	   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_component_open video begin\n",__FUNCTION__);
	   	ret = stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
	   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_component_open video end\n",__FUNCTION__);
	   	if(ret<0){
			av_log(NULL, AV_LOG_ERROR, "%s: stream_component_open video fail\n",__FUNCTION__);
			goto fail;
		}
		this->show_mode = SHOW_MODE_VIDEO;
   	}

   	if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
	   av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_component_open subtitle begin\n",__FUNCTION__);
	   ret = stream_component_open(st_index[AVMEDIA_TYPE_SUBTITLE]);
	   av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_component_open subtitle end\n",__FUNCTION__);
   	}

   	if (this->video_stream < 0 && this->audio_stream < 0) {
	   av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
			  this->filename);
	   ret = -1;
	   goto fail;
   	}

   	
   	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s success\n",__FUNCTION__);

   	return ret;
fail:
    /* close each stream */
    if (this->audio_stream >= 0)
        stream_component_close(this->audio_stream);
    if (this->video_stream >= 0)
        stream_component_close(this->video_stream);
    if (this->subtitle_stream >= 0)
        stream_component_close(this->subtitle_stream);
    if (this->ic) {
        avformat_close_input(&this->ic);
    }
	av_log(NULL, AV_LOG_ERROR, "%s fail\n",__FUNCTION__);

	return ret;
}

int MediaPlayerHard::stream_start()
{
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

	/////try here
	if(this->audio_stream>=0){
		LYH_PauseAudio(this->hardware_context,1); 
	}
	if(this->video_stream>=0){
		this->video_tid = LYH_CreateThread(video_thread, this);
		if(0==this->video_tid){
			av_log(NULL, AV_LOG_ERROR, "%s: create video thread fail\n",__FUNCTION__);
			return -1;
		}else{
			av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: create video thread success(%lu)\n",__FUNCTION__,this->video_tid);
		}
	}
	if(this->subtitle_stream >=0){
		this->subtitle_tid = LYH_CreateThread(subtitle_thread, this);
		if(0==this->subtitle_tid){
			av_log(NULL, AV_LOG_ERROR, "%s: create subtitle thread fail\n",__FUNCTION__);
			return -1;
		}else{
			av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: create subtitle thread success(%lu)\n",__FUNCTION__,this->subtitle_tid);
		}
	}
	/////
	
	if(this->read_tid==0){
		this->read_tid = LYH_CreateThread(read_thread, this);
		if (0==this->read_tid) {
			av_log(NULL, AV_LOG_ERROR, "%s: create read thread fail\n",__FUNCTION__);
			return -1;
		}else{
			av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: create read thread success(%lu)\n",__FUNCTION__,this->read_tid);
		}
	}
	
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
	return 0;
}

int MediaPlayerHard::stream_open(AVInputFormat *iformat)
{
    //MediaPlayer *is;
	int ret;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

	ret = videostate_init(iformat);
    if( ret < 0 ){
		av_log(NULL, AV_LOG_ERROR, "%s: videostate_init() fail\n",__FUNCTION__);
		return -1;
	}

	//ret = video_open(is,0,NULL);
	//if( ret < 0 ){
	//	return -1;
	//}

	ret = do_stream_open();
	if( ret < 0 ){
		av_log(NULL, AV_LOG_ERROR, "%s: do_stream_open() fail\n",__FUNCTION__);
		return -1;
	}

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
    return 0;
}


void MediaPlayerHard::refresh_loop_wait_event(LYH_event *event) 
{
    double remaining_time = 0.0;
	int ret;
    /*
    while (!LYH_PeepEvents(is->hardware_context,event, LYH_ALLEVENTS)) {
        if (remaining_time > 0.0){
            av_usleep((int64_t)(remaining_time * 1000000.0));
        }
        remaining_time = REFRESH_RATE;
        if ( (is->show_mode != SHOW_MODE_NONE) && is->buffer_full && (!is->paused || is->force_refresh)){ //if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh)){
            video_refresh(is, &remaining_time);
        }
    }*/
     do{
	 	ret = LYH_PeepEvents(this->hardware_context,event, LYH_ALLEVENTS);
		if(1==ret){
			if(event->type==LYH_SEEK_EVENT && this->seeking){
				av_log(NULL, AV_LOG_DEBUG, "%s: push seeking event when seeking\n",__FUNCTION__);
				LYH_PushEvent(this->hardware_context,event); 
			}else{
				break;
			}
		}
        if (remaining_time > 0.0){
            av_usleep((int64_t)(remaining_time * 1000000.0));
        }
        remaining_time = REFRESH_RATE;
        if ( (this->show_mode == SHOW_MODE_VIDEO) && this->buffer_full && (!this->paused || this->force_refresh)){ //if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh)){
            video_refresh(&remaining_time);
        }
    }while(1);
}

///new add by lyh
int MediaPlayerHard::seek_to(int msec)
{
  	int64_t ts;
	
	ts = (long long)msec;
	ts = ts * 1000; //ts * 1000000;

	//av_log(NULL, AV_LOG_INFO, "%s: into, sizeof(int64_t)=%d,msec=%ld,lld_ts=%lld,f_ts=%f,x_ts=%0x\n",__FUNCTION__,sizeof(int64_t),msec,ts,(double)ts,ts);

	if (this->ic->start_time != AV_NOPTS_VALUE){
		//av_log(NULL, AV_LOG_INFO, "%s: cur_stream->ic->start_time=%ld\n",__FUNCTION__,this->ic->start_time);
		//ts += cur_stream->ic->start_time;
		if(ts<this->ic->start_time){
			ts += this->ic->start_time;
		}
    }
    stream_seek(ts, 0, 0);

	return 0;
}


///
/* handle an event sent by the GUI */
void MediaPlayerHard::event_loop()
{
    LYH_event event;
    double incr, pos; ///frac;
	int msec; ///
	int ret ;

    for (;;) {
        double x;
        refresh_loop_wait_event(&event);
        switch (event.type) {
        case LYH_QUIT_EVENT: 
        	av_log(NULL, AV_LOG_DEBUG, "%s: recv a quit event\n",__FUNCTION__);
            do_exit(1);
			return ;
		case LYH_RESET_EVENT: 
        	av_log(NULL, AV_LOG_DEBUG, "%s: recv a reset event\n",__FUNCTION__);
            do_exit(0);
			return ;
        case LYH_ALLOC_EVENT:
			av_log(NULL, AV_LOG_DEBUG, "%s: recv a alloc picture event\n",__FUNCTION__);
            ret = alloc_picture();
			if(0!=ret){
				av_log(NULL, AV_LOG_ERROR, "%s: alloc picture fail\n",__FUNCTION__);
				do_exit(1);
				return ;
			}
            break;
		case LYH_PAUSE_EVENT:
			av_log(NULL, AV_LOG_DEBUG, "%s: recv a pause event\n",__FUNCTION__);
			toggle_pause();
			break;
		case LYH_SEEK_EVENT:
			av_log(NULL, AV_LOG_DEBUG, "%s: recv a seek event\n",__FUNCTION__);
			//if(cur_stream->seeking){
			//	av_log(NULL, AV_LOG_WARNING, "%s: now is seeking, wait\n",__FUNCTION__);
			//	LYH_PushEvent(cur_stream->hardware_context,&event); 
			//}else{
				if(!this->seeking){
					av_log(NULL, AV_LOG_WARNING, "%s: set seeking to 1\n",__FUNCTION__);
					this->seeking = 1;
				}
				msec = *(int*)event.data;
				seek_to(msec);
			//}
			break;
		case LYH_SET_VOLUME_EVENT:
			msec = (int)event.data;
			av_log(NULL, AV_LOG_DEBUG, "%s: recv a set VOLUME event,volume=%d\n",__FUNCTION__,msec);
			if(NULL!=this->hardware_context && NULL!=this->hardware_context->audio_device_android_opensles){
				this->hardware_context->audio_device_android_opensles->SetSpeakerVolume(msec);
			}
			break;
        default:
			av_log(NULL, AV_LOG_DEBUG, "%s: unknow event\n",__FUNCTION__);
            break;
        }
        
    }
}



void* MediaPlayerHard::event_thread(void *arg)
{
	MediaPlayerHard *is = (MediaPlayerHard *)arg;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: event thread into\n",__FUNCTION__);

	is->event_loop();

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: event thread exit\n",__FUNCTION__);
	return 0;
}

int MediaPlayerHard::lockmgr(void **mtx, enum AVLockOp op)
{
   switch(op) {
      case AV_LOCK_CREATE:
          *mtx = LYH_CreateMutex(); 
          if(!*mtx)
              return 1;
          return 0;
      case AV_LOCK_OBTAIN:
          return  !!LYH_LockMutex((LYH_Mutex*)*mtx);
      case AV_LOCK_RELEASE:
          return !!LYH_UnlockMutex((LYH_Mutex*)*mtx); 
      case AV_LOCK_DESTROY:
          LYH_DestroyMutex((LYH_Mutex*)*mtx);
          return 0;
   }
   return 1;
}


void* MediaPlayerHard::stream_open_thread(void *arg)
{
	int ret;
	MediaPlayerHard *is = (MediaPlayerHard *)arg;

	//av_log_set_callback(player_log_callback_help);

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream open thread into\n",__FUNCTION__);
	
	avcodec_register_all(); ///fail here only in a ZTE mobile
	av_register_all();
	avformat_network_init();

	if (av_lockmgr_register(lockmgr)) {
		av_log(NULL, AV_LOG_FATAL, "Could not initialize lock manager!\n");
		if(is->state==SPALYER_STATE_PREPARING){
			goto fail;
		}
		is->stream_open_thread_id  = 0;
		return NULL;
	}

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_open() begin\n",__FUNCTION__);
	ret = is->stream_open(NULL);
	if (ret<0) {
		av_log(NULL, AV_LOG_FATAL, "%s: Failed to stream_open()!\n",__FUNCTION__);
		if(is->state==SPALYER_STATE_PREPARING){
			av_log(NULL, AV_LOG_ERROR, "%s: notify IO error\n",__FUNCTION__);
			goto fail;
		}
		is->stream_open_thread_id  = 0;
		return NULL; //return SPLAYER_ERROR_NETWORK;
	}
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: stream_open() end\n",__FUNCTION__);

	///event thread, handle event and show video 有BUG，应该先创建?
	///is->event_thread_id = LYH_CreateThread(event_thread, is); 

	is->state = SPALYER_STATE_PREPARED;

	is->notify(MEDIA_PREPARED, 0,0 , NULL);

	is->stream_open_thread_id = 0;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: success, stream open thread exit\n",__FUNCTION__);

	return is;

fail:
	if(!is->abort_request){ //主动关闭则不需要报错
		is->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_IO ,0);
	}

	is->stream_open_thread_id  = 0;
	
	av_log(NULL, AV_LOG_ERROR, "%s: fail, stream open thread exit\n",__FUNCTION__);
	return NULL;
}

//extern int player_debug_level;

void MediaPlayerHard::player_log_callback_help(void *ptr, int level, const char *fmt, va_list vl)
{
    if(level <= STrace::Filter()){
    	char buf[1024];
    	vsprintf(buf, fmt, vl);  
		STrace::Log(level,"%s",buf);
    }
}



/************************************** Interface*********************************/

MediaPlayerHard::MediaPlayerHard()
{
	this->read_tid = 0;
	this->video_tid = 0;
	this->iformat = NULL;
	this->abort_request = 0;
	this->force_refresh = 0;
	this->paused = 0;
	this->last_paused = 0;
	this->queue_attachments_req = 0;
	this->seek_req = 0;
	this->seek_flags = 0;
	this->seek_pos = 0;
	this->seek_rel = 0;
	this->read_pause_return = 0;
	this->ic = NULL;
	this->realtime = 0;
	this->audio_finished = 0;
	this->video_finished = 0;
	memset((void*)&audclk,0,sizeof(Clock));
	memset((void*)&vidclk,0,sizeof(Clock));
	///Clock extclk;
	this->audio_stream = 0;
	this->av_sync_type = 0;
	this->audio_clock = 0;
	this->audio_clock_total = 0;
	this->audio_clock_serial = 0;
	this->audio_diff_cum  = 0;
	this->audio_diff_avg_coef = 0;
	this->audio_diff_threshold = 0;
	this->audio_diff_avg_count = 0;
	this->audio_st = NULL;
	memset((void*)&audioq,0,sizeof(PacketQueue));
	this->audio_hw_buf_size = 0;
	memset((void*)&silence_buf,0,sizeof(silence_buf));
	this->audio_buf = NULL;
	this->audio_buf1 = NULL;
	this->audio_buf_size = 0;
	this->audio_buf1_size = 0;
	this->audio_buf_index = 0;
	this->audio_write_buf_size = 0;
	this->audio_buf_frames_pending= 0;
	memset((void*)&audio_pkt_temp,0,sizeof(AVPacket));
	memset((void*)&audio_pkt,0,sizeof(AVPacket));
	this->audio_pkt_temp_serial = 0;
	this->audio_last_serial = 0;
	memset((void*)&audio_src,0,sizeof(struct AudioParams)); 
	memset((void*)&audio_tgt,0,sizeof(struct AudioParams)); 
	this->swr_ctx = NULL;
	this->frame_drops_early = 0;
	this->frame_drops_late = 0;
	this->frame = NULL;
	this->audio_frame_next_pts = 0;
	this->show_mode = SHOW_MODE_NONE;
	this->subtitle_tid = 0;
	this->subtitle_stream = 0;
	this->subtitle_st = NULL;
	memset((void*)&subtitleq,0,sizeof(PacketQueue));
	memset((void*)&subpq,0,sizeof(SubPicture)*SUBPICTURE_QUEUE_SIZE);
	this->subpq_size = 0; 
	this->subpq_rindex = 0; 
	this->subpq_windex = 0;
	this->subpq_mutex = NULL;
	this->subpq_cond = NULL;
	this->frame_timer = 0;	
	this->frame_last_pts = 0;
	this->frame_last_serial = 0;
	this->frame_last_duration = 0;
	this->frame_last_dropped_pts = 0;
	this->frame_last_returned_time = 0;
	this->frame_last_filter_delay = 0;
	this->frame_last_dropped_pos = 0;
	this->frame_last_dropped_serial = 0;
	this->video_stream = 0;
	this->video_st = 0;
	memset((void*)&videoq,0,sizeof(PacketQueue));
	this->video_current_pos = 0;
	this->max_frame_duration = 0;
	memset((void*)&pictq,0,sizeof(VideoPicture)*VIDEO_PICTURE_QUEUE_SIZE);
	this->pictq_size = 0; 
	this->pictq_rindex = 0; 
	this->pictq_windex = 0;
	this->pictq_mutex = NULL;
	this->pictq_cond = NULL;
	this->img_convert_ctx = NULL;
	memset((void*)&last_display_rect,0,sizeof(LYH_Rect));
	this->filename = NULL;
	this->width = 0; 
	this->height = 0; 
	this->xleft= 0; 
	this->ytop = 0;
	this->frame_width = 0; 
	this->frame_height = 0;
	this->step = 0;
	this->last_video_stream = 0; 
	this->last_audio_stream = 0; 
	this->last_subtitle_stream = 0;
	this->continue_read_thread = NULL;
	memset((void*)&wanted_stream,0,sizeof(wanted_stream));
	this->audio_disable = 0;
	this->video_disable = 0;
	this->subtitle_disable = 0;
	this->genpts = 0;
	memset((void*)&flush_pkt,0,sizeof(AVPacket));
	this->format_opts = NULL;
	this->codec_opts = NULL;
	this->resample_opts = NULL;
	this->screen = NULL;
	this->event_thread_id = 0;
	this->jvm = 0;
	this->buffer_full = 0;
	this->state = SPALYER_STATE_ERROR;
	this->scaling_mode = VIDEO_SCALING_MODE_SCALE_TO_FIT;
	this->looping = 0;
	this->seek_msec = 0;
	//soft decoder
	this->fast = 0;
	this->lowres = 0;
	this->loop = 0; 
	this->framedrop = 0;
	this->skip_frame = 0;
	this->skip_idct = 0;
	this->skip_loopfilter = 0;
	this->skip_auto_closed = 0; 
	this->hardware_context = NULL;
	this->duration = 0;
	this->laset_buffer_percent = 0; 
	this->MAX_QUEUE_SIZE = 0;
	this->MAX_FRAMES = 0;
	this->mediaplayer_listener = NULL;
	this->mp_listener_lock = NULL;
	this->eof = 0;
	this->stream_open_thread_id = 0;
	this->opengl2_render_listener = NULL;
	this->last_notify_msg = 0;
	this->send_seek_complete_notify = 0;
	this->seeking = 0;
	this->frame_drops_early_continue = 0;
	this->frame_drops_late_continue = 0;
	this->frame_drop_step = 0;
	//hrad decoder
	this->surface_render_listener = NULL;
	this->video_decode_frame = NULL;
	this->video_decode_frame_show = 0;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: create MediaPlayerHard(%p)\n",__FUNCTION__,(void*)this);
	
}
MediaPlayerHard::~MediaPlayerHard()
{
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: destroy MediaPlayerHard(%p) into\n",__FUNCTION__,(void*)this);

	if(this->mediaplayer_listener){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: destroy mediaplayer mediaplayer_listener\n",__FUNCTION__);
		delete this->mediaplayer_listener;
		this->mediaplayer_listener = NULL;
	}

	//if(this->opengl2_render_listener){
	//	av_log(NULL, AV_LOG_WARNING, "%s: destroy mediaplayer opengl2_render_listener\n",__FUNCTION__);
	//	delete this->opengl2_render_listener;
	//	this->opengl2_render_listener = NULL;
	//}

	if(this->surface_render_listener){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: destroy mediaplayer surface_render_listener\n",__FUNCTION__);
		delete this->surface_render_listener;
		this->surface_render_listener = NULL;
	}

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
	
}
int MediaPlayerHard::using_hard_decode()
{
	return 1;
}

int MediaPlayerHard::setup(JavaVM* jvm, jobject thiz, jobject weak_this)
{
	av_log(NULL,RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

	av_log_set_callback(player_log_callback_help);
	av_log_set_flags(AV_LOG_SKIP_REPEATED);
	
	this->looping = 0;
	this->scaling_mode = VIDEO_SCALING_MODE_SCALE_TO_FIT;
	this->filename = NULL;
	this->jvm = jvm;
	
	// create new listener and give it to MediaPlayer
	JNIMediaPlayerListener* listener = new JNIMediaPlayerListener(jvm, thiz, weak_this);
	if (listener == NULL) {
		av_log(NULL, AV_LOG_ERROR, "%s: no memory\n",__FUNCTION__);
		return SPLAYER_ERROR_NO_MEMORY;
	}
	this->mediaplayer_listener	= listener;
		
	this->state = SPALYER_STATE_IDLE;
	
	av_log(NULL,RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
	return SPLAYER_SUCCESS;
}

int MediaPlayerHard::set_surfaceview(JavaVM* jvm,jobject jsurfaceview)
{
	av_log(NULL, AV_LOG_ERROR, "%s: hard decoder unsupport surfaceview\n",__FUNCTION__);
	return SPLAYER_ERROR_SPALYER_STATE;
}

int MediaPlayerHard::set_surface(JavaVM* jvm,void* jsurface)
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	if(!jsurface || !jvm){
		av_log(NULL, AV_LOG_ERROR, "%s: jvm NULL or surface NULL\n",__FUNCTION__);
		return SPLAYER_ERROR_PARAMETER;
	}

	if( this->state != SPALYER_STATE_IDLE ){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}

	JNIAndroidSurfaceRendererListener* listener = new JNIAndroidSurfaceRendererListener(jvm, (jobject)jsurface);
	if (listener == NULL) {
		STrace::Log(SPLAYER_TRACE_ERROR,"%s listener NULL", __FUNCTION__);
        //jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return SPLAYER_ERROR_NO_MEMORY;
	
    }
	
	this->surface_render_listener = listener;

	if(this->filename){
		this->state = SPALYER_STATE_INIT;
	}

	
	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);
	return SPLAYER_SUCCESS;

}

int MediaPlayerHard::set_datasource(const char* path)
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	if(!path){
		av_log(NULL, AV_LOG_ERROR, "%s: path NULL\n",__FUNCTION__);
		return SPLAYER_ERROR_PARAMETER;
	}
	
	if(this->state != SPALYER_STATE_IDLE){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}
	this->filename = (char*)malloc(strlen(path)+1);
	if(!this->filename){
		av_log(NULL, AV_LOG_ERROR, "%s: no memory\n",__FUNCTION__);
		return SPLAYER_ERROR_NO_MEMORY;
	}
	memset(this->filename,0,strlen(path)+1);
	strncpy(this->filename,path,strlen(path));

	if(this->surface_render_listener ){ //|| is->opengl2_render_listener
		this->state = SPALYER_STATE_INIT;
	}
	
	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);
	return SPLAYER_SUCCESS;

}

int MediaPlayerHard::set_video_scaling_mode(int mode)
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into, mode=%d\n",__FUNCTION__,mode);
	
	this->scaling_mode = (enum video_scaling_mode)mode;

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);
	return SPLAYER_SUCCESS;
}

int MediaPlayerHard::set_looping(int loop)
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	this->looping = loop;

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);
	return SPLAYER_SUCCESS;
}


int MediaPlayerHard::prepare()
{
	int ret;

	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	if(	this->state != SPALYER_STATE_INIT &&
		this->state != SPALYER_STATE_PREPARED &&
		this->state != SPALYER_STATE_STOPPED){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}

	if(this->state == SPALYER_STATE_PREPARED ){
		av_log(NULL, AV_LOG_WARNING, "%s: already prepared\n",__FUNCTION__);
		return SPLAYER_SUCCESS;
	}

	////add here
	///解决多次点击播放按钮
	///event thread, handle event and show video 有BUG，应该先创建?
	if(this->hardware_context==NULL){
		this->hardware_context = new LYH_Context();
		this->hardware_context->lyh_event_queue_mtx = NULL;
		this->hardware_context->audio_device_android_opensles = NULL;
	}
	this->event_thread_id = LYH_CreateThread(event_thread, this); 
	if(0==this->event_thread_id){
		av_log(NULL, AV_LOG_FATAL, "%s create event thread fail\n",__FUNCTION__);
		return SPLAYER_ERROR_INTERNAL;
	}else{
		av_log(NULL, AV_LOG_ERROR, "%s create event thread success(%lu)\n",__FUNCTION__,this->event_thread_id);
	}

	if( NULL== stream_open_thread(this) ){
		return SPLAYER_ERROR_NETWORK;
	}

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);

    return SPLAYER_SUCCESS;

}


int MediaPlayerHard::prepareAsync()
{
	int ret;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

	if( this->state != SPALYER_STATE_INIT &&
		this->state != SPALYER_STATE_PREPARING &&
		this->state != SPALYER_STATE_STOPPED){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}
	
	if(this->state == SPALYER_STATE_PREPARING ){
		av_log(NULL, AV_LOG_DEBUG, "%s: already preparing\n",__FUNCTION__);
		return SPLAYER_SUCCESS;
	}

	splayer_state old_state = this->state;
	this->state = SPALYER_STATE_PREPARING;

	////add here
	///解决多次点击播放按钮
	///event thread, handle event and show video 有BUG，应该先创建?
	if(this->hardware_context==NULL){
		this->hardware_context = new LYH_Context();
		if(NULL==this->hardware_context){
			this->state = old_state;
			av_log(NULL, AV_LOG_FATAL, "%s no memory\n",__FUNCTION__);
			return SPLAYER_ERROR_INTERNAL;
		}
		this->hardware_context->lyh_event_queue_mtx = NULL;
		this->hardware_context->audio_device_android_opensles = NULL;
	}
	this->event_thread_id = LYH_CreateThread(event_thread, this); 
	if(0==this->event_thread_id){
		this->state = old_state;
		av_log(NULL, AV_LOG_FATAL, "%s create event thread fail\n",__FUNCTION__);
		return SPLAYER_ERROR_INTERNAL;
	}else{
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s create event thread(%lu) success\n",__FUNCTION__,this->event_thread_id);
	}
		

	//this->state = SPALYER_STATE_PREPARING;
	
	this->stream_open_thread_id = LYH_CreateThread(stream_open_thread, this); 
	if(0==this->stream_open_thread_id){
		this->state = old_state;
		av_log(NULL, AV_LOG_FATAL, "%s create stream thread fail\n",__FUNCTION__);
		return SPLAYER_ERROR_INTERNAL;
	}else{
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s create stream thread(%lu) success\n",__FUNCTION__,this->stream_open_thread_id);
	}

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);

    return SPLAYER_SUCCESS;

}


int MediaPlayerHard::start()
{
	int ret ;
	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	if(	this->state != SPALYER_STATE_PREPARED && 
		this->state != SPALYER_STATE_PAUSED && 
		this->state != SPALYER_STATE_COMPLETED &&
		this->state != SPALYER_STATE_STARTED ){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}
	if(this->state == SPALYER_STATE_STARTED){
		av_log(NULL, AV_LOG_WARNING, "%s: already start\n",__FUNCTION__);
		return SPLAYER_SUCCESS;
	}
	
	if( this->state == SPALYER_STATE_PREPARED ){
		av_log(NULL, AV_LOG_WARNING, "%s: on SPALYER_STATE_PREPARED state\n",__FUNCTION__);
		ret = stream_start();
    	if( ret < 0 ){
			return SPLAYER_ERROR_NORMAL;
		}
	}else if( this->state == SPALYER_STATE_PAUSED ){
		av_log(NULL, AV_LOG_WARNING, "%s: on SPALYER_STATE_PAUSED state\n",__FUNCTION__);
		if(this->seeking){
			//av_log(NULL, AV_LOG_ERROR, "%s: seeking, just return\n",__FUNCTION__);
			return SPLAYER_SUCCESS;
		}
		LYH_event event;
		event.type = LYH_PAUSE_EVENT;
		event.data = NULL; 	
		LYH_PushEvent(this->hardware_context,&event); 
	}else if( this->state == SPALYER_STATE_COMPLETED ){
		av_log(NULL, AV_LOG_WARNING, "%s: on SPALYER_STATE_COMPLETED state\n",__FUNCTION__);
		LYH_event event;
		//this->seeking = 1;
		this->send_seek_complete_notify = 0;
		this->seek_msec  = 0;
		event.type = LYH_SEEK_EVENT;
		event.data = (void*)&this->seek_msec; // seek_frac; 	
		LYH_PushEvent(this->hardware_context,&event); 
	}else{
		av_log(NULL, AV_LOG_WARNING, "%s: on unhandle %d  state\n",__FUNCTION__,this->state);
	}
	
	this->state = SPALYER_STATE_STARTED;

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);

    return SPLAYER_SUCCESS;

}

int MediaPlayerHard::pause()
{
	int ret ;
	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	if( this->state != SPALYER_STATE_PAUSED && 
		this->state != SPALYER_STATE_STARTED ){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}
	if(this->state == SPALYER_STATE_PAUSED){
		av_log(NULL, AV_LOG_WARNING, "%s: already paused\n",__FUNCTION__);
		return SPLAYER_SUCCESS;
	}
	
	LYH_event event;

	event.type = LYH_PAUSE_EVENT;
    event.data = NULL; 
    
    LYH_PushEvent(this->hardware_context,&event); 
	
	this->state = SPALYER_STATE_PAUSED;

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);

	return SPLAYER_SUCCESS;

}

int MediaPlayerHard::stop()
{
	int ret ;
	av_log(NULL, AV_LOG_ERROR, "%s: into\n",__FUNCTION__);

	if( this->state != SPALYER_STATE_PAUSED && 
		this->state != SPALYER_STATE_STARTED &&
		this->state != SPALYER_STATE_PREPARED &&
		this->state != SPALYER_STATE_COMPLETED &&
		this->state != SPALYER_STATE_STOPPED){
			av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
			return SPLAYER_ERROR_SPALYER_STATE;
	}
	if(this->state == SPALYER_STATE_STOPPED){
		av_log(NULL, AV_LOG_WARNING, "%s: already stopped\n",__FUNCTION__);
		return SPLAYER_SUCCESS;
	}
	
	int looping = this->looping;
	enum video_scaling_mode scaling_mode = this->scaling_mode;
	char * filename = (char*)malloc(strlen(this->filename)+1);
	memset(filename,0,strlen(this->filename)+1);
	strncpy(filename,this->filename,strlen(this->filename));
	
	LYH_event event; 
	event.type = LYH_RESET_EVENT;
	event.data = NULL; 
	
	LYH_PushEvent(this->hardware_context,&event); 

	LYH_WaitThread(this->event_thread_id,NULL);
		
	
	this->looping = looping;
	this->scaling_mode = scaling_mode;
	this->filename = (char*)malloc(strlen(filename)+1);
	memset(this->filename,0,strlen(filename)+1);
	strncpy(this->filename,filename,strlen(filename)+1);

	this->state = SPALYER_STATE_STOPPED;
		
	free(filename);

	av_log(NULL, AV_LOG_ERROR, "%s: out\n",__FUNCTION__);

	return SPLAYER_SUCCESS;
}

int MediaPlayerHard::reset()
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	if( this->state == SPALYER_STATE_END){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state\n",__FUNCTION__);
		return SPLAYER_ERROR_SPALYER_STATE;
	}
	
	if(this->state == SPALYER_STATE_IDLE){
		av_log(NULL, AV_LOG_ERROR, "%s: already reset\n",__FUNCTION__);
		return SPLAYER_SUCCESS;
	}

	LYH_event event; 
	event.type = LYH_RESET_EVENT;
	event.data = NULL; 
	
	LYH_PushEvent(this->hardware_context,&event); 

	LYH_WaitThread(this->event_thread_id,NULL);
		
	this->state = SPALYER_STATE_IDLE;
	this->looping = 0;
	this->scaling_mode = VIDEO_SCALING_MODE_SCALE_TO_FIT;

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);

	return SPLAYER_SUCCESS;

}

int MediaPlayerHard::release()
{
	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: into\n",__FUNCTION__);

	if( this->state == SPALYER_STATE_END){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: already release\n",__FUNCTION__);
		return SPLAYER_SUCCESS;
	}

	this->abort_request = 1;  //IO block
	//this->state = SPALYER_STATE_END;

	//if(this->stream_open_thread_id !=0 ){
		av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: waiting stream open thread exit\n",__FUNCTION__);
		LYH_WaitThread(this->stream_open_thread_id,NULL);
	//}

	LYH_event event;	
	event.type = LYH_QUIT_EVENT;
	event.data = NULL; 
	
	LYH_PushEvent(this->hardware_context,&event); 

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: waiting event thread(%ld) exit\n",__FUNCTION__,this->event_thread_id);
	LYH_WaitThread(this->event_thread_id,NULL);
	//av_log(NULL, AV_LOG_ERROR, "%s: wait event thread exit end\n",__FUNCTION__);

	
	this->state = SPALYER_STATE_END;

	av_log(NULL, RELEASE_DEBUG_LOG_LEVEL, "%s: out\n",__FUNCTION__);
	return SPLAYER_SUCCESS;

}

int MediaPlayerHard::seekto(int msec)
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into, msec=%d,is->seeking=%d\n",__FUNCTION__,msec,this->seeking);

	LYH_event event;

	if(	this->state!=SPALYER_STATE_STARTED && 
		this->state!=SPALYER_STATE_PAUSED && 
		this->state!=SPALYER_STATE_COMPLETED &&
		this->state!=SPALYER_STATE_PREPARED){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}

	if(this->state==SPALYER_STATE_PREPARED){
		if(this->read_tid == 0){
			this->read_tid = LYH_CreateThread(read_thread, this);
			if (0==this->read_tid) {
				av_log(NULL, AV_LOG_ERROR, "%s: create read thread fail\n",__FUNCTION__);
				return SPLAYER_ERROR_NORMAL;
			}else{
				av_log(NULL, AV_LOG_ERROR, "%s: create read thread success(%lu)\n",__FUNCTION__,this->read_tid);
			}
		}
	}
	//if(1==is->seeking){
	//	av_log(NULL, AV_LOG_WARNING, "%s: now is seeking\n",__FUNCTION__);
	//	return SPLAYER_SUCCESS;
	//}
	//is->seeking = 1;
	pause();
	
	this->seek_msec  = msec;
	this->send_seek_complete_notify = 1;
	event.type = LYH_SEEK_EVENT;
	event.data = (void*)&this->seek_msec; // seek_frac; 
	
	LYH_PushEvent(this->hardware_context,&event); 

	if(this->state==SPALYER_STATE_COMPLETED){
		av_log(NULL, AV_LOG_DEBUG, "%s: state from COMPLETED to STARTED\n",__FUNCTION__);
		this->state = SPALYER_STATE_STARTED;
	}
	
	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);
	
	return SPLAYER_SUCCESS;
}

int MediaPlayerHard::is_playing()
{
	//av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);
		
	return (this->state==SPALYER_STATE_STARTED);
}

int MediaPlayerHard::get_video_width()
{
	return this->frame_width;
}

int MediaPlayerHard::get_video_height()
{
	return this->frame_height;
}

float MediaPlayerHard::get_video_aspect_ratio()
{
	if(this->frame_width<=0 || this->frame_height<=0){
		return 0;
	}
	float w_ = (float)this->frame_width;
	float h_ = (float)this->frame_height;
	
	return w_/h_;
}


int MediaPlayerHard::get_current_position()
{
	int msec = 0;
	if( is_realtime(this->ic) ){
		msec = this->audio_clock_total * 1000;
	}else{
		msec = this->audio_clock * 1000;
	}
	
	if(msec<0){
		av_log(NULL, AV_LOG_ERROR, "%s: is->audio_clock not set yet\n",__FUNCTION__);
		msec = 0;
	}
	//av_log(NULL, AV_LOG_DEBUG, "%s: into is->audio_clock=%f, msec=%d\n",__FUNCTION__,this->audio_clock,msec);
	return msec;

}
int MediaPlayerHard::get_duration()
{
	int msec = this->duration/1000 ;
	if(msec<0){
		av_log(NULL, AV_LOG_ERROR, "%s: is->duration not set yet\n",__FUNCTION__);
		msec = 0;
	}
	//av_log(NULL, AV_LOG_DEBUG, "%s: into is->duration=%ld, msec=%d\n",__FUNCTION__,this->duration,msec);
	return msec;
}

int MediaPlayerHard::is_looping()
{
	//av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);
	
	return this->looping;
}


int MediaPlayerHard::is_buffering()
{
	//av_log(NULL, AV_LOG_DEBUG, "%s: into\n",__FUNCTION__);

	return !this->buffer_full;
}

int MediaPlayerHard::setVolume(float left, float right)
{
	av_log(NULL, AV_LOG_DEBUG, "%s: into,left=%f,right=%f\n",__FUNCTION__,left,right);

	if( this->state != SPALYER_STATE_PREPARED && 
		this->state != SPALYER_STATE_PAUSED && 
		this->state != SPALYER_STATE_STARTED &&
		this->state != SPALYER_STATE_COMPLETED ){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong state,is->state=%d\n",__FUNCTION__,this->state);
		return SPLAYER_ERROR_SPALYER_STATE;
	}

	if(left<0 || left>1){
		av_log(NULL, AV_LOG_ERROR, "%s: wrong left volume range\n",__FUNCTION__);
		return	SPLAYER_ERROR_PARAMETER;
	}
	// [ 0 -1 ] to [ -8000 -0 ] 
	left = left * 8000 - 8000;
	int vol = (int)left;
	
	LYH_event event;

	event.type = LYH_SET_VOLUME_EVENT;
    event.data = (void*)vol; 
    
    LYH_PushEvent(this->hardware_context,&event); 

	av_log(NULL, AV_LOG_DEBUG, "%s: out\n",__FUNCTION__);

}

int MediaPlayerHard::config(const char* cfg, int val)
{
	if(strncmp(cfg,"fast",4)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set fast=%d\n",__FUNCTION__,val);
		this->fast = val;
	}else if(strncmp(cfg,"framedrop",9)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set framedrop=%d\n",__FUNCTION__,val);
		this->framedrop = val;
	}else if(strncmp(cfg,"lowres",6)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set lowres=%d\n",__FUNCTION__,val);
		this->lowres = val;
	}else if(strncmp(cfg,"skip",4)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set skip_frame=%d\n",__FUNCTION__,val);
		this->skip_frame = val;
	}else if(strncmp(cfg,"idct",4)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set skip_idct=%d\n",__FUNCTION__,val);
		this->skip_idct = val;
	}else if(strncmp(cfg,"filter",6)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set skip_loopfilter=%d\n",__FUNCTION__,val);
		this->skip_loopfilter= val;
	}else if(strncmp(cfg,"debug",5)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set debug=%d\n",__FUNCTION__,val);
		STrace::SetFilter(val);
	}else if(strncmp(cfg,"buffer",6)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set buffer=%d\n",__FUNCTION__,val);
		int frames = val / 40;
		if(frames>=0){
			this->MAX_FRAMES = frames;
		}
		//this->MAX_QUEUE_SIZE = val;
	}else if(strncmp(cfg,"auto",4)==0){
		av_log(NULL, AV_LOG_INFO, "%s: set skip_auto_closed=%d\n",__FUNCTION__,val);
		this->skip_auto_closed = val; 
	}else{
		av_log(NULL, AV_LOG_INFO, "%s: not support config=%s\n",__FUNCTION__,cfg);
		return -1;
	}
	return 0;
}


void  MediaPlayerHard::notify(int msg, int ext1, int ext2, void *obj)
{    	
	switch (msg) {    
		case MEDIA_NOP: // interface test message        break;   
			av_log(NULL, AV_LOG_DEBUG,"nop");   
			break;
		case MEDIA_PREPARED:        
			av_log(NULL, AV_LOG_DEBUG,"prepared");      
			break;    
		case MEDIA_PLAYBACK_COMPLETE:        
			if(this->state == SPALYER_STATE_COMPLETED){
				av_log(NULL, AV_LOG_DEBUG,"%s: already in SPALYER_STATE_COMPLETED state",__FUNCTION__);
				return;
			}
			if(this->last_notify_msg == MEDIA_PLAYBACK_COMPLETE){
				av_log(NULL, AV_LOG_DEBUG,"%s: already send MEDIA_PLAYBACK_COMPLETE msg",__FUNCTION__);
				return;
			}
			av_log(NULL, AV_LOG_DEBUG,"playback complete");
			this->state = SPALYER_STATE_COMPLETED;      
			break;    
		case MEDIA_ERROR:        
			// Always log errors.        
			// ext1: Media framework error code.        
			// ext2: Implementation dependant error code.        
			av_log(NULL, AV_LOG_DEBUG,"error (%d, %d)", ext1, ext2);        
		
			break;    
		case MEDIA_INFO:        
			// ext1: Media framework error code.        
			// ext2: Implementation dependant error code.        
			//if (ext1 != MEDIA_INFO_VIDEO_TRACK_LAGGING) {            
			av_log(NULL, AV_LOG_DEBUG,"info/warning (%d, %d)", ext1, ext2);        
			//}        
			break;    
		case MEDIA_SEEK_COMPLETE:        
			av_log(NULL, AV_LOG_DEBUG,"Received seek complete");          
			break;    
		case MEDIA_BUFFERING_UPDATE:        
			av_log(NULL, AV_LOG_DEBUG,"buffering %d", ext1);        
			break;    
		case MEDIA_SET_VIDEO_SIZE:        
			av_log(NULL, AV_LOG_DEBUG,"New video size %d x %d", ext1, ext2);        
			//mVideoWidth = ext1;        
			//mVideoHeight = ext2;        
			break;    
		default:        
			av_log(NULL, AV_LOG_DEBUG,"unrecognized message: (%d, %d, %d)", msg, ext1, ext2);        
			break;    
	}
	

	if (this->mediaplayer_listener != 0) {        
		LYH_LockMutex(this->mp_listener_lock); 
		this->mediaplayer_listener->notify(msg, ext1, ext2, obj);   
		LYH_UnlockMutex(this->mp_listener_lock); 
	}

	this->last_notify_msg = msg;
}



