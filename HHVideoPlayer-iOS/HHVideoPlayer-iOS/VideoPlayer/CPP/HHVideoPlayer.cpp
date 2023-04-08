//
//  HHVideoPlayer.cpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/8.
//

#include "HHVideoPlayer.h"
#include <thread>
#include <iostream>

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
//    初始化解码后的帧队列 FrameQueue
//    ret = frame_queue_init(&iv->pictq, &iv->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1);
//    ret = frame_queue_init(&iv->sampq, &iv->audioq, SAMPLE_QUEUE_SIZE, 1);
      
//    初始化解码前的帧队列 PacketQueue
    ret = packet_queue_init(&iv->videoq);
    ret = packet_queue_init(&iv->audioq);
   
//    初始化 Decoder
//    iv->auddec = d
    
    if(!(iv->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
//        return NULL;
    }
    
    //初始化时钟
//    init_clock(&iv->vidclk, &iv->videoq.serial);
//    init_clock(&iv->audclk, &iv->audioq.serial);
//    init_clock(&iv->extclk, &iv->extclk.serial);
    iv->audio_clock_serial = -1;
    
    iv->av_sync_type = av_sync_type;// 音频时钟同步
//    iv->read_tid = SDL_CreateThread(read_thread, "read_thread", iv);
   
    if (!iv->read_tid) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
    }
    is = iv;
}
 
void HHVideoPlayer::setState(int state){
    if(state == is->state) return;
//    is->state = static_cast<VideoState::state>(state);
//    stateChanged(self);    //通知播放器进行UI改变
}

HHVideoPlayer::HHVideoPlayer() {
//    cout<< "初始化～～～" <<endl ;
    printf("初始化～～～");
    initVideoState();
    
    av_init_packet(&flush_pkt);
    flush_pkt.data = (uint8_t *)&flush_pkt;
}

HHVideoPlayer::~HHVideoPlayer() {
    printf("析构函数～～～～");
}

void HHVideoPlayer::initSwr() {
    is->_aSwrOutSpec.sampleRate = is->audioCodecCtx->sample_rate;
    is->_aSwrInSpec.sampleFmt = is->audioCodecCtx->sample_fmt;
    is->_aSwrInSpec.chLayout = static_cast<int>(is->audioCodecCtx->channel_layout);
    is->_aSwrInSpec.chs = is->audioCodecCtx->channels;
    
    is->_aSwrOutSpec.sampleRate = SAMPLE_RATE;
    is->_aSwrOutSpec.sampleFmt = AV_SAMPLE_FMT_S16;
    
}

void HHVideoPlayer::play() {
//    if (is->state == Playing) return;;
    
    if (is->state == Stopped) {
        std::thread([this](){
            readFile();
        }).detach();
    }else {
        setState(Playing);
    }
    
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
//    is->haveVideo = initVideoInfo();
    is->aCodec = avcodec_find_decoder(aStream->codecpar->codec_id);
    is->vCodec = avcodec_find_decoder(vStream->codecpar->codec_id);
    is->audioCodecCtx = avcodec_alloc_context3(is->aCodec);
    is->videoCodecCtx = avcodec_alloc_context3(is->vCodec);
     
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
    while (is->state != Stopped) {
        int vSize = is->videoq.size;
        int aSize = is->audioq.size;
//        if (vSize + aSize  > 10000) {
//            SDL_Delay(10);
//            cout<< " 缓存满了～～～～ " << endl ;
//            continue;; // 缓存足够大了就缓存
//        }
          
        av_format_inject_global_side_data(is->ic);
        ret = av_read_frame(is->ic, &aPacket);
        index ++;
        
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
//                addAudioPkt(&aPacket);
//                cout << &aPacket << "音频音频音频音频音频" << index  << endl;
            }else if (aPacket.stream_index == is->video_stream) {
                addVideoPkt(&aPacket);
                cout << &aPacket << "视频视频视频视频" << index  << endl;
            }else {
//                av_packet_unref(aPacket);
            }
        }
    }
    
    SDL_DestroyMutex(wait_mutex);
}

void HHVideoPlayer::packet_queue_put_private(PacketQueue *q, AVPacket *pkt) {
    MyAVPacketList *pkt1;
    pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));
    pkt1->pkt = *pkt;
    pkt1 -> next = NULL;
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
    cout<< "音频帧大小～～" << pkt1->pkt.size <<endl;
}


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

void HHVideoPlayer::setFilename(const char *filename){
    memcpy(_filename,filename,strlen(filename) + 1);
    cout << _filename << endl;
}

bool HHVideoPlayer::initAudioInfo() {
//    int ret = initDecoder(<#AVCodecContext **decodeCtx#>, <#AVStream **stream#>, <#AVMediaType type#>)
    
    return true;
}

bool HHVideoPlayer::initVideoInfo() {
    
    return true;
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
    q->abort_request = 1;
    return 0;
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

int HHVideoPlayer::audio_thread(void *arg)
{
    AVFrame *frame = av_frame_alloc();
    Frame *af;
    int got_frame = 0;
    AVRational tb;
    int ret = 0;
     
    if (!frame)
        return AVERROR(ENOMEM);
    
    do {
        got_frame = HHVideoPlayer::decoder_decode_frame(&is->auddec, frame, NULL);
        if (got_frame) {
            tb = (AVRational){1, frame->sample_rate};
            
        }
    }while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
    
    return 0;
}

int HHVideoPlayer::stream_component_open(VideoState *tis, int stream_index) {
    AVFormatContext *ic = tis->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    const char *forced_codec_name = NULL;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = 0;
    
    if (stream_index <0 || stream_index >= ic->nb_streams) {
        return  -1;
    }
    avctx = avcodec_alloc_context3(NULL);
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
            ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt);
           
            is->audio_hw_buf_size = ret;
            is->audio_src = is->audio_tgt;
            is->audio_buf_size = 0;
            is->audio_buf_index = 0;
            is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
            is->audio_diff_avg_count = 0;
            is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;
            is->audio_stream = stream_index;
            is->audio_st = ic->streams[stream_index];
            decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
            if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
                is->auddec.start_pts = is->audio_st->start_time;
                is->auddec.start_pts_tb = is->audio_st->time_base;
            }
            
            
//            ret = decoder_start(&is->auddec, audio_thread, "audio_decoder", is);
//            packet_queue_init(is->auddec.queue);
//            is->auddec.decoder_tid = SDL_CreateThread(audio_thread(), "audio_decoder", is);
            
//            int HHVideoPlayer::decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void *arg)
//            packet_queue_start(d->queue);
//            d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
//            if (!d->decoder_tid) {
//                return AVERROR(ENOMEM);
//            }
        }
            
            break;
        case AVMEDIA_TYPE_VIDEO: {
            
        }
            break;
    }
    
    return ret;
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
//    swr_alloc_set_opts(<#struct SwrContext *s#>, <#int64_t out_ch_layout#>, <#enum AVSampleFormat out_sample_fmt#>, <#int out_sample_rate#>, <#int64_t in_ch_layout#>, <#enum AVSampleFormat in_sample_fmt#>, <#int in_sample_rate#>, <#int log_offset#>, <#void *log_ctx#>)
    //    av_samples_alloc(outFrame->data, outFrame->linesize, out_chs, 4096, out_samplefmt, 1);
    return 0;
}
 

int HHVideoPlayer::initVideoSwr() {
    
    return 0;
} 

void HHVideoPlayer::setSelf(void *aSelf)
{
    self = aSelf;
}


int HHVideoPlayer::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params) {
    
    int wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
    
    
    return 1;
}

void HHVideoPlayer::decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) {
    
}

void HHVideoPlayer::packet_queue_start(PacketQueue *q)
{
    SDL_LockMutex(q->mutex); // 锁定互斥锁，保证线程安全。
    q->abort_request = 0; // 清除队列中止标志位，表示队列可以开始正常运行。
    packet_queue_put_private(q, &flush_pkt); // 在队列中加入一个名为flush_pkt的数据包，作为队列的起始标志，可以清空队列中的所有数据
    SDL_UnlockMutex(q->mutex); // 解锁互斥锁，释放线程安全控制。
}

int HHVideoPlayer::decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void *arg) {
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid) {
        return AVERROR(ENOMEM);
    }
    return 0;
}

int HHVideoPlayer::decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    
    return 0;
}
 
