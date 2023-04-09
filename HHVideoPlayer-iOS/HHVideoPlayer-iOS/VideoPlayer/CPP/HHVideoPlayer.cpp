//
//  HHVideoPlayer.cpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/8.
//

#include "HHVideoPlayer.h"
#include <thread>
#include <SDL_thread.h>
#include <iostream>

#include "videoPlayer.h"

#define SAMPLE_RATE 44100 // 采样率
#define SAMPLE_FORMAT AUDIO_S16LSB// 采样格式
#define SAMPLE_SIZE SDL_AUDIO_BITSIZE(SAMPLE_FORMAT)// 采样大小
#define CHANNELS 2 // 声道数
#define SAMPLES 512 // 音频缓冲区的样本数量
  
static AVPacket flush_pkt;
 
void HHVideoPlayer::initVideoState() {
    VideoState *iv =  (VideoState *)av_malloc(sizeof(VideoState));
    int ret;
    iv->last_video_stream = iv->video_stream = -1;// 初始化 视频 stream头尾标记
    iv->last_audio_stream = iv->audio_stream = -1;// 初始化 音频 stream头尾标记
    iv->filename = _filename;
    iv->ic = avformat_alloc_context();
    
    ret = frame_queue_init(&iv->pictq, &iv->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1);// 初始化解码后的帧队列 FrameQueue
    ret = frame_queue_init(&iv->sampq, &iv->audioq, SAMPLE_QUEUE_SIZE, 1);
      
    ret = packet_queue_init(&iv->videoq);// 初始化解码前的帧队列 PacketQueue
    ret = packet_queue_init(&iv->audioq);
   
 
    if(!(iv->continue_read_thread = SDL_CreateCond())) { // Create  read_thread
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
    }
     
    init_clock(&iv->vidclk, &iv->videoq.serial);    //初始化时钟
    init_clock(&iv->audclk, &iv->audioq.serial);
    init_clock(&iv->extclk, &iv->extclk.serial);
    iv->audio_clock_serial = -1;
    
    iv->av_sync_type = av_sync_type;// 音频时钟同步
   
    if (!iv->read_tid) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
    }
    is = iv;
}
 
void HHVideoPlayer::setState(int state) {
    if(state == is->state) return;
//    is->state = static_cast<VideoState::state>(state);
//    stateChanged(self);    //通知播放器进行UI改变
}

HHVideoPlayer::HHVideoPlayer() {
    printf("初始化～～～");
    initVideoState();
    
    av_init_packet(&flush_pkt);
    flush_pkt.data = (uint8_t *)&flush_pkt;
}

HHVideoPlayer::~HHVideoPlayer() {
    printf("析构函数～～～～");
}

void HHVideoPlayer::setSelf(void *aSelf) {
    self = aSelf;
}

#pragma mark - Init Method
void HHVideoPlayer::initSwr() {
    is->_aSwrOutSpec.sampleRate = is->audioCodecCtx->sample_rate;
    is->_aSwrInSpec.sampleFmt = is->audioCodecCtx->sample_fmt;
    is->_aSwrInSpec.chLayout = static_cast<int>(is->audioCodecCtx->channel_layout);
    is->_aSwrInSpec.chs = is->audioCodecCtx->channels;
    
    is->_aSwrOutSpec.sampleRate = SAMPLE_RATE;
    is->_aSwrOutSpec.sampleFmt = AV_SAMPLE_FMT_S16;
}

bool HHVideoPlayer::initAudioInfo() {
    for (int i = 0; i< is->ic->nb_streams; i++) {
        AVStream *st = is->ic->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            return true;
        }
    }
    return false;
}

bool HHVideoPlayer::initVideoInfo() {
    for (int i = 0; i< is->ic->nb_streams; i++) {
        AVStream *st = is->ic->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            return true;
        }
    }
    return false;
}


int HHVideoPlayer::initDecoder(AVCodecContext **decodeCtx, AVStream **stream, AVMediaType type) {
   int ret = av_find_best_stream(is->ic, type, -1, -1, nullptr, 0);
   return 0;
}

int HHVideoPlayer::initAudioSwr() {
   int ret = 0;
   // 输入采样率
   int in_sample_rate = is->audioCodecCtx->sample_rate;
   AVSampleFormat in_sp_fmt = is->audioCodecCtx->sample_fmt;
   int in_channel_layout = is->audioCodecCtx->channel_layout;
   int in_channels = is->audioCodecCtx->channels;
   
   // 输出采样率
   int outSampleRate = SAMPLE_RATE;
   int out_samplefmt= AV_SAMPLE_FMT_S16;
   int out_chLayout = AV_CH_LAYOUT_STEREO;
   int out_chs = av_get_channel_layout_nb_channels(is->audioCodecCtx->channel_layout);
   int out_bytesPerSampleFrame = out_chs * av_get_bytes_per_sample(is->audioCodecCtx->sample_fmt);
   
   SwrContext *aSwrCtx = nullptr;    //音频重采样
//    aSwrCtx = swr_alloc_set_opts(nullptr, out_chLayout, out_samplefmt, outSampleRate, in_channel_layout, in_sp_fmt, in_sample_rate, 0, nullptr);
//    swr_alloc_set_opts(aSwrCtx, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
   ret = swr_init(aSwrCtx);
   
    is->_aSwrInFrame = av_frame_alloc();
    is->_aSwrOutFrame = av_frame_alloc();
    ret = av_samples_alloc(is->_aSwrOutFrame->data, is->_aSwrOutFrame->linesize, is->_aSwrOutSpec.chs, 4096, is->_aSwrOutSpec.sampleFmt, 1);
 
//    swr_alloc_set_opts(aSwrCtx, out_chLayout, out_samplefmt, outSampleRate, in_channel_layout, in_sp_fmt, in_sample_rate, 0, nullptr);
//    swr_alloc_set_opts(<#struct SwrContext *s#>, <#int64_t out_ch_layout#>, <#enum AVSampleFormat out_sample_fmt#>, <#int out_sample_rate#>, <#int64_t in_ch_layout#>, <#enum AVSampleFormat in_sample_fmt#>, <#int in_sample_rate#>, <#int log_offset#>, <#void *log_ctx#>)
   //    av_samples_alloc(outFrame->data, outFrame->linesize, out_chs, 4096, out_samplefmt, 1);
   return 0;
}


int HHVideoPlayer::initVideoSwr() {
   
   return 0;
}

void HHVideoPlayer::decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) {
    memset(d, 0, sizeof(Decoder));
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
    d->pkt_serial = -1;
}

int HHVideoPlayer::packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if (!q->cond) {
        return AVERROR(ENOMEM);
    }
//    q->abort_request = 1;
    cout<< "初始化 ～～PacketQueue 与 mutex & cond " <<endl;
    return 0;
}

int HHVideoPlayer::frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last) {
    int i ;
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = SDL_CreateMutex())) {
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = SDL_CreateCond())) {
        return AVERROR(ENOMEM);
    }
    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    for (i = 0; i< f->max_size; i++) {
        if (!(f->queue[i].frame = av_frame_alloc())) {
            return AVERROR(ENOMEM);
        }
    }
    cout << " 初始化 FrameQueue &  PacketQueue " << endl;
    return 0;
}

#pragma mark -
void HHVideoPlayer::play() {
//    if (is->state == Playing) return;;
    
//    if (is->state == Stopped) {
        std::thread([this](){
            readFile();
        }).detach();
        
//        std::thread([this]() {
//            event_loop(is);
//        }).detach();
//    }else {
//        setState(Playing);
//    }
    
}

void HHVideoPlayer::readFile() {
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    AVFormatContext *fmtCtx = is->ic;
    int ret = 0;
    ret = avformat_open_input(&fmtCtx, _filename, nullptr, nullptr);
    ret = avformat_find_stream_info(fmtCtx, nullptr);
    
    int audioIndex = av_find_best_stream(is->ic, AVMEDIA_TYPE_AUDIO, AVMEDIA_TYPE_AUDIO, -1, nullptr, 0);
    int videoIndex = av_find_best_stream(is->ic, AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_VIDEO, -1, nullptr, 0);
    AVStream *aStream = is->ic->streams[audioIndex];
    AVStream *vStream = is->ic->streams[videoIndex];
    is->audio_st = aStream;
    is->video_st = vStream;
    is->audio_stream = audioIndex;
    is->video_stream = videoIndex;
    
    is->haveAudio = initAudioInfo();
    is->haveVideo = initVideoInfo();
    is->aCodec = avcodec_find_decoder(aStream->codecpar->codec_id);
    is->vCodec = avcodec_find_decoder(vStream->codecpar->codec_id);
    cout<< "初始化～～～Codec" <<endl;
    is->audioCodecCtx = avcodec_alloc_context3(is->aCodec);
    is->videoCodecCtx = avcodec_alloc_context3(is->vCodec);
    cout<< "初始化 ～～～～AVCodecContext " << endl;
     
    ret = avcodec_parameters_to_context(is->audioCodecCtx,aStream->codecpar);
    ret = avcodec_parameters_to_context(is->videoCodecCtx, vStream->codecpar);
    
    ret = avcodec_open2(is->audioCodecCtx, is->aCodec, nullptr);
    ret = avcodec_open2(is->videoCodecCtx, is->vCodec, nullptr);
    
    if (!is->haveAudio && !is->haveVideo) {
        cout<< "没有音频～～～～～没有视频啊啊啊啊啊" << endl ;
        return;
    }
    is->state = Playing;
//    SDL_PauseAudio(0);
    
    if (is->haveAudio) {
        stream_component_open(is, is->audio_stream);
    }
//    if (is->haveVideo) {
//        stream_component_open(is, is->video_stream);
//    }
    
//    AVFrame *inFrame = av_frame_alloc();
//    AVFrame *outFrame = av_frame_alloc();
    AVPacket aPacket; 
    int index = 0;
    int audioCout = 0;
    int videoCout = 0 ;
    
    while (is->state != Stopped) {
//        int vSize = is->videoq.size;
//        int aSize = is->audioq.size;
//        if (vSize + aSize  > 500) {
//            SDL_Delay(10);
//            cout<< " 缓存满了～～～～ " << endl ;
//            continue;; // 缓存足够大了就缓存
//        }
          
        av_format_inject_global_side_data(is->ic);
        ret = av_read_frame(is->ic, &aPacket);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(is->ic->pb)) && !is->eof) {
                if (is->video_stream >= 0) {
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                }
                if (is->audio_stream >= 0) {
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                }
                is->eof = 1;
                cout << "读取到文件末尾了～～" << endl ;
                SDL_LockMutex(wait_mutex);
                SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
                SDL_UnlockMutex(wait_mutex);
                continue;
            }
        } else {
            is->eof = 0;
        }
         
        if (ret == 0) {
            if (aPacket.stream_index == is->audio_stream) {
                audioCout = audioCout + 1; 
                addAudioPkt(&aPacket);
                cout << &aPacket << "音频音频音频音频音频" << is->audioq.serial  << endl;
            }else if (aPacket.stream_index == is->video_stream) {
                videoCout = videoCout + 1;
                addVideoPkt(&aPacket);
//                cout << &aPacket << "视频视频视频视频" << index  << endl;
            }else {
                av_packet_unref(&aPacket);
            }
        }
    }
    
    SDL_DestroyMutex(wait_mutex);
}

void HHVideoPlayer::setFilename(const char *filename) {
    memcpy(_filename,filename,strlen(filename) + 1);
    cout << _filename << endl;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial) {
    MyAVPacketList *pkt1;
    int ret; // 定义变量pkt1和ret，分别用于存储数据包的地址和函数返回值
    SDL_LockMutex(q->mutex); // SDL_LockMutex(q->mutex)：锁定队列的互斥锁，保证线程安全
    for (;;) { // 循环处理数据包获取请求，直到获取到数据包或者队列被中止。
        pkt1 = q->first_pkt; // 判断队列中是否有数据包可用，如果有，取出队列中的第一个数据包并更新队列信息。
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            q->duration -= pkt1->pkt.duration;
            cout << "取出一个数据包～～大小: " << pkt->size <<"哈哈哈哈" << "时间" << endl;
            *pkt = pkt1->pkt;
            if (serial)
                *serial = pkt1->serial;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) { // 如果没有数据包可用，且不需要阻塞等待，则返回0。
            ret = 0;
            break;
        } else { // 如果没有数据包可用，但需要阻塞等待，则调用SDL_CondWait函数等待条件变量的信号。
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex); // SDL_UnlockMutex(q->mutex)：解锁队列的互斥锁，释放线程安全控制。
    return ret;
}

//int decoderAvcodeSendPacket(Decoder *d, AVPacket pkt) {
//    int ret = 0;
//    ret = avcodec_send_packet(d->avctx, &pkt);
//    if (ret == AVERROR(EAGAIN)) {
//        cout<< " 发送到解码器缓冲区失败 " <<endl;
//    }else if (ret == 0) {
//        cout<< " 发送到解码器成功 " << "size ==== " << pkt.size  <<endl;
//    }else if (ret == AVERROR_EOF) {
//        cout<< " 已经读到文件结尾，解码完成 " <<endl;
//    }else {
//        cout << "其他错误，需要根据具体错误码进行处理 " <<endl;
//    }
//    return ret;
//}
//
//int decoderAvcodecReceiveFrame(Decoder *d, AVFrame *frame) {
//    int ret = 0;
//    ret = avcodec_receive_frame(d->avctx, frame);
//    if (ret == AVERROR(EAGAIN)) {
//        cout<< " 解码器缓冲区中没有可用的数据帧 " <<endl;
//    }else if (ret == 0) {
//        cout<< " 成功从解码器中获取数据帧 " << "pkt_size === " << frame->pkt_size <<endl;
//    }else if (ret == AVERROR_EOF) {
//        cout<< " 已经读到文件结尾，解码完成 " <<endl;
//    }else {
//        cout << "其他错误，需要根据具体错误码进行处理 " <<endl;
//    }
//    return ret;
//}


int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    int ret = AVERROR(EAGAIN);
    int send_ret = 0;
    int receive_ret = 0;
    int sendIndex = 0;
    int receiveIndex = 0;
    int  total = 0;
//    for (; ; ) {
//        AVPacket pkt;
//        do {
//            if (d->queue->nb_packets == 0) { // 没有待解码的包
//                SDL_CondSignal(d->empty_queue_cond);
//            }
//            if (d->packet_pending) { //如果 pending =1 说明之前有在解码，移动到pkt 中
//                av_packet_move_ref(&pkt, &d->pkt);
//                d->packet_pending = 0;
//            }else {
//                ret = packet_queue_get(d->queue, &pkt, 1, &d->pkt_serial);// 从queue中取出一个包
//                if (ret < 0) {
//                    return -1;
//                }
//            }
//            if(d->queue->serial == d->pkt_serial) {
//                break;
//            }
//            av_packet_unref(&pkt);
//        } while (1);
//
//        if (pkt.data == flush_pkt.data) {
//            avcodec_flush_buffers(d->avctx);
//            d->finished = 0;
//            d->next_pts = d->start_pts;
//            d->next_pts_tb = d->start_pts_tb;
//            cout<< "第一针～～～～～～～～～" << endl;
//        }else {
//            if (d->avctx->codec_type == AVMEDIA_TYPE_VIDEO ||
//                d->avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
//                send_ret = avcodec_send_packet(d->avctx, &pkt);
//                cout<< "send_ret == " << send_ret << endl;
//                if (send_ret == 0) {
//                    total = total + pkt.size;
//                    cout<< "pkt.size == "<< pkt.size << "total:" << total <<endl;
//                }else {
//                    cout<< " 错误～～ ret:"<< ret << endl;
//                }
////                ret = decoderAvcodeSendPacket(d->avctx, &pkt);
////                receive_ret = avcodec_receive_frame(d->avctx, frame);
////                cout<< "receive_ret == " << receive_ret << endl;
////                if (send_ret == 0 && receive_ret == 0) {
////                    total = total + pkt.size;
////                    cout<< "pkt.size == "<< pkt.size << "frame.size === " << frame->pkt_size << "total:" << total <<endl;
////                }
////                ret = decoderAvcodecReceiveFrame(d->avctx, frame);
////                cout<< "🍊🍊🍊 ====receiveIndex =" << receiveIndex << ",ret ==== " << ret << endl;
//            }else {
//                cout<< " 失败了？？？？？？？？？？？？？？  " <<endl;
//            }
//            av_packet_unref(&pkt);
//        }
//    }
    
    return -1;
}

static void frame_queue_push(FrameQueue *f) {
    if (++f->windex == f->max_size)
        f->windex = 0;
    SDL_LockMutex(f->mutex);
    f->size++;
    SDL_CondSignal(f->cond); // 它会将写入位置（windex）向前移动一个位置，并检查是否已经移动到队列的末尾。如果已经到了队列末尾，它会将写入位置（windex）重置为 0。
    SDL_UnlockMutex(f->mutex);
    cout << "frame_queue_push 解码后的大小事多少啊啊啊" <<  f->size << endl;
}


static Frame *frame_queue_peek_writable(FrameQueue *f) { // ，它会在帧队列中有空间可用之前一直等待，并阻塞当前线程，直到有新的帧可被写入队列
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size &&!f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);
  
    return &f->queue[f->windex];
}

int audio_decoder_thread(void *avg) {
    VideoState *vs = (VideoState *)avg;
   
    AVFrame *frame = av_frame_alloc(); // 用于存储解码信息，通常用于音视频解码器的解码操作
    Frame *af; // 用来存储解码后的音频帧
    int last_serial = -1; //表示上一个解码数据包的序列号
    int reconfigure; // 表示是否需要对视频解码器进行重新配置
    int got_frame = 0; // 表示当前解码操作是否成功。如果解码操作成功，got_frame 会被置为1，否则为0。
    AVRational tb; // 视频解码时，视频帧的时间戳需要以该时间单位表示
    int ret = 0;
     
    cout<< "解码～～～～～解码～～～～解码～～～～" << endl;
//
//    got_frame = decoder_decode_frame(&vs->auddec, frame, NULL);
//    do {
////        cout<< "decoder_decode_frame -----------" << endl;
////        if (got_frame) {
////            tb = (AVRational){1, frame->sample_rate};
////            af = frame_queue_peek_writable(&vs->sampq);
////            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
////            af->pos = frame->pkt_pos;
////            af->serial = vs->auddec.pkt_serial;
////            af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});
////            av_frame_move_ref(af->frame, frame);
////            frame_queue_push(&vs->sampq);
////        }
//    }while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
    
//    do {
//        if ((got_frame = decoder_decode_frame(&vs->auddec, frame, NULL)) < 0)
//            goto the_end;
//        if (got_frame) {
//            tb = (AVRational){1, frame->sample_rate};
//            if (!(af = frame_queue_peek_writable(&vs->sampq)))
//                goto the_end;
//            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
//            af->pos = frame->pkt_pos;
//            af->serial = vs->auddec.pkt_serial;
//            af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});
//            av_frame_move_ref(af->frame, frame);
//            frame_queue_push(&vs->sampq);
//        }
//    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
    
the_end:
    cout<< "aaaa哈哈哈哈哈" << endl;
    return 0;
}

#pragma mark - open Method  
#pragma mark - Decoder 初始化～～～～
int HHVideoPlayer::stream_component_open(VideoState *tis, int stream_index) {
    AVFormatContext *ic = tis->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    const char *forced_codec_name = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = 0;
    
    if (stream_index <0 || stream_index >= ic->nb_streams) {
        return  -1;
    }
    avctx = is->audioCodecCtx;//avcodec_alloc_context3(NULL);
    ret = avcodec_open2(avctx, codec, nullptr);
    
    if (!avctx) {
        return AVERROR(ENOMEM);
    }
    
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0) {
        return -1;
    }
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;
    codec = avcodec_find_decoder(avctx->codec_id);
    
    if (forced_codec_name) {
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    }
    avctx->codec_id = codec->id;
    if (stream_lowres > codec->max_lowres) {
        stream_lowres = codec->max_lowres;
    }
    avctx->lowres = stream_lowres;
     
    is->eof = 0;
    
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO: {
            sample_rate    = avctx->sample_rate;
            nb_channels    = avctx->channels;
            channel_layout = avctx->channel_layout;
//            ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt);
            // 不知何用～～～～～
//            is->audio_hw_buf_size = ret;
            is->audio_src = is->audio_tgt;
            is->audio_buf_size = 0;
            is->audio_buf_index = 0;
            is->audio_stream = stream_index;
            is->audio_st = ic->streams[stream_index];
            // 不知何用～～～～～
            decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread); // Decoder 初始化
            ret = decoder_start(&is->auddec);
#pragma mark 解码解码解码
        }
            break;
        case AVMEDIA_TYPE_VIDEO: {
            
        }
            break;
    }
    
    return ret;
}


#pragma mark  - Addddddddddddd
void HHVideoPlayer::addAudioPkt(AVPacket *pkt) {
    SDL_LockMutex(is->audioq.mutex);
    packet_queue_put_private(&is->audioq, pkt);
    SDL_UnlockMutex(is->audioq.mutex);
}

void HHVideoPlayer::addVideoPkt(AVPacket *pkt) {
    SDL_LockMutex(is->videoq.mutex);
    packet_queue_put_private(&is->videoq, pkt);
    SDL_UnlockMutex(is->videoq.mutex);
}

#pragma mark - Queue ------
void HHVideoPlayer::packet_queue_put_private(PacketQueue *q, AVPacket *pkt) {
    MyAVPacketList *pkt1;
    pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));
    pkt1->pkt = *pkt;
    pkt1 -> next = NULL;
    q->serial ++;
    pkt1->serial = is->audioq.serial;
    if (!q->last_pkt) {
        q->first_pkt = pkt1;
    }else {
        q->last_pkt->next = pkt1;
    }
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->duration += pkt1->pkt.duration;
//    cout<< "音频帧大小～～" << pkt1->pkt.size << "序号：" << pkt1->serial <<endl;
}

int  HHVideoPlayer::packet_queue_put_nullpacket(PacketQueue *q, int stream_index) {
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    packet_queue_put_private(q, pkt);
    return 1;
}

void HHVideoPlayer::packet_queue_start(PacketQueue *q) {
    SDL_LockMutex(q->mutex); // 锁定互斥锁，保证线程安全。
//    q->abort_request = 0; // 清除队列中止标志位，表示队列可以开始正常运行。
    packet_queue_put_private(q, &flush_pkt); // 在队列中加入一个名为flush_pkt的数据包，作为队列的起始标志，可以清空队列中的所有数据
    SDL_UnlockMutex(q->mutex); // 解锁互斥锁，释放线程安全控制。
}

#pragma mark - decoder ~~~~
int HHVideoPlayer::decoder_start(Decoder *d) {
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(audio_decoder_thread, "audio_decoder_thread", is);
    if (!d->decoder_tid) {
        return AVERROR(ENOMEM);
    }
    return 0;
}
  

#pragma mark - clock
static void set_clock_at(Clock *c, double pts, int serial, double time) {
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

static void set_clock(Clock *c, double pts, int serial) {
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

void HHVideoPlayer::init_clock(Clock *c, int *queue_serial) {
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

