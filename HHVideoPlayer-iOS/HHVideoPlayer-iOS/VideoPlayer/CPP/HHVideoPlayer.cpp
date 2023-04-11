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
#define SDL_AUDIO_MIN_BUFFER_SIZE 512 // 表示 SDL 音频缓冲区的最小大小
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30 // 表示音频回调的最大次数
#define AV_NOSYNC_THRESHOLD 10.0 // 表示如果同步错误太大，则不会进行音视频同步
#define SAMPLE_CORRECTION_PERCENT_MAX 10 // 表示音频速度变化的最大值。

static int64_t audio_callback_time;

static AVPacket flush_pkt;

int readFile(void *arg) {
    
    return 0;
}

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
    initSDL();
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
    aSwrOutSpec.sampleRate = is->audioCodecCtx->sample_rate;
    aSwrInSpec.sampleFmt = is->audioCodecCtx->sample_fmt;
    aSwrInSpec.chLayout = static_cast<int>(is->audioCodecCtx->channel_layout);
    aSwrInSpec.chs = is->audioCodecCtx->channels;

    aSwrOutSpec.sampleRate = SAMPLE_RATE;
    aSwrOutSpec.sampleFmt = AV_SAMPLE_FMT_S16;
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

int HHVideoPlayer::initAudioSwr() {
   int ret = 0;
   int in_sample_rate = is->audioCodecCtx->sample_rate;   // 输入采样率
   AVSampleFormat in_sp_fmt = is->audioCodecCtx->sample_fmt;
   int in_channel_layout = is->audioCodecCtx->channel_layout;
   int in_channels = is->audioCodecCtx->channels;
 
   int outSampleRate = SAMPLE_RATE;    // 输出采样率
   int out_samplefmt= AV_SAMPLE_FMT_S16;
   int out_chLayout = AV_CH_LAYOUT_STEREO;
   int out_chs = av_get_channel_layout_nb_channels(is->audioCodecCtx->channel_layout);
   int out_bytesPerSampleFrame = out_chs * av_get_bytes_per_sample(is->audioCodecCtx->sample_fmt);
   
   SwrContext *aSwrCtx = nullptr;    //音频重采样
   ret = swr_init(aSwrCtx);
    is->_aSwrInFrame = av_frame_alloc();
    is->_aSwrOutFrame = av_frame_alloc();
//    ret = av_samples_alloc(is->_aSwrOutFrame->data, is->_aSwrOutFrame->linesize, is->_aSwrOutSpec.chs, 4096, is->_aSwrOutSpec.sampleFmt, 1);
//    swr_alloc_set_opts(aSwrCtx, out_chLayout, out_samplefmt, outSampleRate, in_channel_layout, in_sp_fmt, in_sample_rate, 0, nullptr);
//       av_samples_alloc(outFrame->data, outFrame->linesize, out_chs, 4096, out_samplefmt, 1);
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
    
    AVPacket pkt1, *pkt = &pkt1;
    int64_t stream_start_time;
    int64_t pkt_ts;
    int pkt_in_play_range = 0;
    
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
    SDL_PauseAudio(0);
    
    if (is->haveAudio) {
        stream_component_open(is, is->audio_stream); // 获取视频信息，初始化audio swr
    }
    
//    if (is->haveVideo) {
//        stream_component_open(is, is->video_stream);
//    }
    AVPacket aPacket; 
    int index = 0;
    int audioCout = 0;
    int videoCout = 0 ;
    
    
    
    for (;;) {
        if (is->abort_request) {
            break;
        }
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused) {
                is->read_pause_return = av_read_pause(is->ic);
            }else {
                av_read_play(is->ic);
            }
        }
        bool enoughtSize = is->audioq.size + is->videoq.size > MAX_QUEUE_SIZE; //总容量满了
        bool enoughtPackets = stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq) && stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq);// 帧满了 or 时间满了
        if (infinite_buffer < 1 && (enoughtSize||enoughtPackets)) {
            cout<< "～～～～等待等待等待～～～～～～～～～enoughtSize～～"<< enoughtSize <<"~~enoughtPackets:" << enoughtPackets << endl;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        
        if (!is->paused &&
            (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
            (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))) {
//            if (loop != 1 && (!loop || --loop)) {
//                stream_seek(is, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
//            } else if (autoexit) {
//                ret = AVERROR_EOF;
//                goto fail;
//            }
        }
        
        ret = av_read_frame(is->ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(is->ic->pb)) && !is->eof) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                is->eof = 1;
            }
            if (is->ic->pb && is->ic->pb->error)
                break;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
        }else {
            is->eof = 0;
        }
        
        stream_start_time = is->ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = duration == AV_NOPTS_VALUE || (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) * av_q2d(is->ic->streams[pkt->stream_index]->time_base) - (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000 <= ((double)duration / 1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
            cout<< "给数据～～～～～～～～🍎" << endl;
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                   && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(&is->videoq, pkt);
            cout<< "给数据～～～～～～～～🍊" << endl;
        }  else {
            av_packet_unref(pkt);
        }
    }
    
//-=-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-=
//    while (is->state != Stopped) {
////        int vSize = is->videoq.size;
////        int aSize = is->audioq.size;
////        if (vSize + aSize  > 500) {
////            SDL_Delay(10);
////            cout<< " 缓存满了～～～～ " << endl ;
////            continue;; // 缓存足够大了就缓存
////        }
//
//        av_format_inject_global_side_data(is->ic);
//        ret = av_read_frame(is->ic, &aPacket);
//        if (ret < 0) {
//            if ((ret == AVERROR_EOF || avio_feof(is->ic->pb)) && !is->eof) {
//                if (is->video_stream >= 0) {
//                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
//                }
//                if (is->audio_stream >= 0) {
//                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
//                }
//                is->eof = 1;
//                cout << "读取到文件末尾了～～" << endl ;
//                stream_component_openA(is, is->audio_stream);
//                SDL_LockMutex(wait_mutex);
//                SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
//                SDL_UnlockMutex(wait_mutex);
//                continue;
//            }
//        } else {
//            is->eof = 0;
//        }
//
//        if (ret == 0) {
//            if (aPacket.stream_index == is->audio_stream) {
//                audioCout = audioCout + 1;
//                addAudioPkt(&aPacket);
//                cout << &aPacket << "音频音频音频音频音频" << is->audioq.serial  << endl;
//            }else if (aPacket.stream_index == is->video_stream) {
//                videoCout = videoCout + 1;
////                addVideoPkt(&aPacket);
////                cout << &aPacket << "视频视频视频视频" << index  << endl;
//            }else {
//                av_packet_unref(&aPacket);
//            }
//        }
//    }
    
//    SDL_DestroyMutex(wait_mutex);
}

void HHVideoPlayer::setFilename(const char *filename) {
    memcpy(_filename,filename,strlen(filename) + 1);
    cout << _filename << endl;
}

/* seek in the stream */
void HHVideoPlayer::stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes) { // 执行媒体流的跳转（seek）操作。
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    }
}

int HHVideoPlayer::frame_queue_nb_remaining(FrameQueue *f) { // 段代码用于获取帧队列（FrameQueue）中尚未读取的帧数
    return f->size - f->rindex_shown;
}


int HHVideoPlayer::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

int HHVideoPlayer::packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;
    SDL_LockMutex(q->mutex);// 该函数会使用一个互斥锁来保证线程安全，然后调用 packet_queue_put_private 函数将数据包添加到队列中
    ret = packet_queue_put_private(q, pkt);
    SDL_UnlockMutex(q->mutex);
    if (pkt != &flush_pkt && ret < 0) // 如果该函数返回负数，表示数据包添加失败，此时会将数据包的引用计数减一以释放资源。
        av_packet_unref(pkt); // 判断 pkt != &flush_pkt 的条件是为了避免释放 flush_pkt 这个空数据包（其实是一种特殊的数据包，用于表示“清空队列”）
    return ret;
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
        cout<< "packet_queue_get ～～ 一直在循环" << endl;
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
//    int ret = AVERROR(EAGAIN);
//    int send_ret = 0;
//    int receive_ret = 0;
//    int sendIndex = 0;
//    int receiveIndex = 0;
//    int  total = 0;
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

static Frame *frame_queue_push(FrameQueue *f) {
    if (++f->windex == f->max_size)
        f->windex = 0;
    SDL_LockMutex(f->mutex);
    f->size++;
    SDL_CondSignal(f->cond); // 它会将写入位置（windex）向前移动一个位置，并检查是否已经移动到队列的末尾。如果已经到了队列末尾，它会将写入位置（windex）重置为 0。
    SDL_UnlockMutex(f->mutex);
    cout << "frame_queue_push 解码后的大小事多少啊啊啊" <<  f->size << endl;
    
    return &f->queue[(f->rindex + f->rindex_shown)% f->max_size];
}

static void frame_queue_unref_item(Frame *vp) {
    av_frame_unref(vp->frame);
//    avsubtitle_free(&vp->sub);
}

void HHVideoPlayer::frame_queue_next(FrameQueue *f) {
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    SDL_LockMutex(f->mutex);
    f->size--;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
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
//    VideoState *vs = (VideoState *)avg;
//
//    AVFrame *frame = av_frame_alloc(); // 用于存储解码信息，通常用于音视频解码器的解码操作
//    Frame *af; // 用来存储解码后的音频帧
//    int last_serial = -1; //表示上一个解码数据包的序列号
//    int reconfigure; // 表示是否需要对视频解码器进行重新配置
//    int got_frame = 0; // 表示当前解码操作是否成功。如果解码操作成功，got_frame 会被置为1，否则为0。
//    AVRational tb; // 视频解码时，视频帧的时间戳需要以该时间单位表示
//    int ret = 0;
     
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

#pragma mark - SDL
int HHVideoPlayer::initSDL() {
    
    int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (SDL_Init(flags)) { // 返回值不是0，就代表失败
        cout << "SDL_Init Error ～～" << SDL_GetError() << endl;
    }
    
//    SDL_SetMainReady();
//    SDL_AudioSpec spec;
//    spec.freq = 44100;
//    spec.format =  AUDIO_S16SYS;
//    spec.channels = 2;
//    spec.samples = 1024;
//    spec.userdata = this;
//    spec.callback = sdlAudioCallbackFunc;
//    spec.userdata = this;
//    if (SDL_OpenAudio(&spec, nullptr)) {
//        cout<< " SDL_OpenAudio Error " <<endl;
//        return  -1;
//    }
//    SDL_PauseAudio(0);
    return 0;
}

void HHVideoPlayer::sdlAudioCallbackFunc(void *userdata, Uint8 *stream, int len) {
    HHVideoPlayer *player = (HHVideoPlayer *)userdata;
    player->sdlAudioCallback(stream, len);
}

void HHVideoPlayer::sdlAudioCallback(Uint8 *stream, int len) {
    SDL_memset(stream, 0, len);
    while (len > 0) {
        if(aSwrOutIdx >= aSwrOutSize) {
            aSwrOutSize = decodeAudio(); // 解码出来算大小
            aSwrOutIdx = 0;
            if (aSwrOutSize <= 0) {
                aSwrOutSize = 1024; // 假定1024个字节
                memset(aSwrOutFrame->data[0], 0, aSwrOutSize);
            }
        }
        int fillLen = aSwrOutSize - aSwrOutIdx;
        fillLen = std::min(fillLen,len);
        int volumn = 100;
        SDL_MixAudio(stream, aSwrOutFrame->data[0] + aSwrOutIdx, fillLen, volumn);
        len -= fillLen;
        stream += fillLen;
        aSwrOutIdx = fillLen;
    }
}

int HHVideoPlayer::decodeAudio() {
    AVPacket pkt;
    int ret = 0;
    AVFrame *frame = av_frame_alloc();
    
    pkt = is->audioq.first_pkt->pkt;
    MyAVPacketList *first_pkt = is->audioq.first_pkt;
    MyAVPacketList *next = first_pkt->next;
    pkt = first_pkt->pkt;
    ret = avcodec_send_packet(is->audioCodecCtx, &pkt);
    cout << "ret ==== "<< ret << endl;
    ret = avcodec_receive_frame(is->audioCodecCtx, frame);
    is->audioq.first_pkt = next;
     
    int64_t outSamples = av_rescale_rnd(SAMPLE_RATE, SAMPLES, SAMPLES, AV_ROUND_UP);
    ret = swr_convert(is->swr_ctx, aSwrOutFrame->data, outSamples, (const uint8_t**)aSwrInFrame->data, aSwrInFrame->nb_samples);
    return ret * aSwrOutSpec.bytesPerSampleFrame;
}

#pragma mark - Decoder 初始化～～～～
int HHVideoPlayer::stream_component_openA(VideoState *tis, int stream_index) {
    cout<< " 开始读取数据～～～～ " <<endl;
    int ret = 0;
    AVPacket pkt;
    AVFrame *frame = av_frame_alloc();
    int receiveIndex = 0;
    do {
        pkt = is->audioq.first_pkt->pkt;
        MyAVPacketList *first_pkt = is->audioq.first_pkt;
        MyAVPacketList *next = first_pkt->next;
        pkt = first_pkt->pkt;
        ret = avcodec_send_packet(is->audioCodecCtx, &pkt);
        cout << "ret ==== "<< ret << endl;
        ret = avcodec_receive_frame(is->audioCodecCtx, frame);
        receiveIndex = receiveIndex + 1;
        if (ret == 0) {
            is->audioq.size = is->audioq.size - pkt.size - sizeof(*first_pkt);
            cout<< "解码成功～～～～" << first_pkt<< "🃏🃏🃏" << is->audioq.size << "~~~～"<< receiveIndex << endl;
            is->audioq.first_pkt = next;
        }else {
            cout<< "解码失败～～～～～" << ret << "🃏🃏🃏" << ret << "~~~～" << receiveIndex << endl;
            is->audioq.first_pkt = next;
        }
        if (ret == AVERROR_EOF) {
            cout<< "释放前～～～～"<< &pkt << endl;
            av_packet_unref(&pkt);
            cout<< "释放成功～～～～"<< &pkt << endl;
        }
    } while (ret != AVERROR_EOF);
     
//    pkt = is->audioq.first_pkt->pkt;
//    MyAVPacketList *first_pkt = is->audioq.first_pkt;
//    ret = avcodec_send_packet(is->audioCodecCtx, &pkt);
//    ret = avcodec_receive_frame(is->audioCodecCtx, frame);
//    if (ret == 0) {
//        cout<< "解码成功～～～～" << endl;
//    }else {
//        cout<< "解码失败～～～～～" << endl;
//    }
    
    if (pkt.dts != AV_NOPTS_VALUE) {
        
    }
    return 1;
}

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
    avctx = is->audioCodecCtx;
    
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
    
    sample_rate    = avctx->sample_rate;//采样率 48000
    nb_channels    = avctx->channels; // 音频通道 2
    channel_layout = avctx->channel_layout; // 音频声道布局3
 
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO: { 
            ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt);
//            if(ret < 0){ ///////////这个地方是要打开的～～～～～
//                cout<< "audio_open  error" <<endl;
//                return 0;
//            }
            is->audio_hw_buf_size = ret;
            is->audio_src = is->audio_tgt;
            is->audio_buf_size = 0;
            is->audio_buf_index = 0;
            is->audio_stream = stream_index;
            is->audio_st = ic->streams[stream_index];
            is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
            is->audio_diff_avg_count = 0;
            is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;
            is->audio_stream = stream_index;
            is->audio_st = ic->streams[stream_index];
            decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread); // Decoder 初始化
            if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
                is->auddec.start_pts = is->audio_st->start_time;
                is->auddec.start_pts_tb = is->audio_st->time_base;
            }
            if ((ret = decoder_start(&is->auddec)) < 0) {
                return ret;
            }
//                goto out;
            SDL_PauseAudioDevice(0, 0);
//            ret = decoder_start(&is->auddec);
//#pragma mark 解码解码解码
            
        }
            break;
        case AVMEDIA_TYPE_VIDEO: {
            
        }
            break;
        case AVMEDIA_TYPE_UNKNOWN:
        case AVMEDIA_TYPE_DATA:
        case AVMEDIA_TYPE_SUBTITLE:
        case AVMEDIA_TYPE_ATTACHMENT:
        case AVMEDIA_TYPE_NB: {
            break;
        }
    }
    return ret;
}


int HHVideoPlayer::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params) {
    SDL_AudioSpec wanted_spec, spec;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
    
    wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
   
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = opaque;
    
    while (!(SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                       "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
     }
    
    if (spec.format != AUDIO_S16SYS) {
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            return -1;
        }
    }
    
    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels =  spec.channels;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    return spec.size;
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
int HHVideoPlayer::packet_queue_put_private(PacketQueue *q, AVPacket *pkt) {
    MyAVPacketList *pkt1;
    pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));
    if (!pkt1) {
        return -1;
    }
    pkt1->pkt = *pkt;
    pkt1 -> next = NULL;
    
    if (pkt == &flush_pkt) {
        q->serial++; //数据流序号
    }
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
   
    SDL_CondSignal(q->cond);
    if (pkt1->pkt.stream_index == 1) {
        cout<< "音频帧大小～～" << q->size <<"啊对对对对"<< "序号：" << q->serial <<endl;
    }
    return 0;
}

int  HHVideoPlayer::packet_queue_put_nullpacket(PacketQueue *q, int stream_index) { //初始化一个空包
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


void HHVideoPlayer::sdl_audio_callback(void *opaque, Uint8 *stream, int len) {
    VideoState *is_t = (VideoState *)opaque;
    int audio_size, len1;
    audio_callback_time = av_gettime_relative();

//    while (len > 0) {
//        if (is->audio_buf_index >= is->audio_buf_size) {
//            audio_size = audio_decode_frame(is);
//            if (audio_size < 0) {
//                is->audio_buf = NULL;
//                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
//            }else {
//                update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
//                is->audio_buf_size = audio_size;
//            }
//            is->audio_buf_index = 0;
//        }
//        len1 = is->audio_buf_size - is->audio_buf_index;
//    }
//    if (len1 > len) {
//        if (is->audio_buf) {
//            memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
//        }else {
//            memset(stream, 0, len1);
//            if (is->audio_buf) {
//                SDL_MixAudioFormat(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, AUDIO_S16SYS, len1, 100);
//            }
//        }
//        len -= len1;
//        stream += len1;
//        is->audio_buf_index = len1;
//    }
//    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
//    if (!isnan(is->audio_clock)) {
//        double ptg_t = is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec;
//        int serial_t = is->audio_clock_serial;
//        double time_t = audio_callback_time / 1000000.0;
//        set_clock_at(&is->audclk, ptg_t, serial_t, time_t);
//        sync_clock_to_slave(&is->extclk, &is->audclk);
//    }
}

void HHVideoPlayer::sync_clock_to_slave(Clock *c, Clock *slave) {
    double clock = get_clock(c); // 时钟（Clock）与从时钟（slave）同步的函数
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

double HHVideoPlayer::get_clock(Clock *c) { // 该函数的作用是获取一个时钟的当前时间
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

int HHVideoPlayer::audio_decode_frame(VideoState *is) {
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af = nullptr;
    
    do {
        if (!(af = frame_queue_push(&is->sampq))) {
            return -1;
        }
        frame_queue_next(&is->sampq);
    } while (af->serial != is->audioq.serial);
    
     
    int nb_channels = af->frame->channels;
    int nb_samples = af->frame->nb_samples;
    AVSampleFormat sample_fmt = static_cast<AVSampleFormat>(af->frame->format);
    int align = 1;
    data_size = av_samples_get_buffer_size(NULL, nb_channels, nb_samples, sample_fmt, 1);
    dec_channel_layout = (af->frame->channel_layout && af->frame->channels == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ? af->frame->channel_layout : av_get_default_channel_layout(af->frame->channels);
    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);
    bool format_comp = af->frame->format != is->audio_src.fmt;
    bool channel_comp = dec_channel_layout != is->audio_src.channel_layout;
    bool sample_comp = af->frame->sample_rate != af->frame->nb_samples;
    if ( format_comp || channel_comp|| (sample_comp && !is->swr_ctx)) {
        swr_free(&is->swr_ctx);
        AVSampleFormat inSampleFmt = static_cast<AVSampleFormat>(af->frame->format);
        is->swr_ctx = swr_alloc_set_opts(NULL, is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq, dec_channel_layout, inSampleFmt, af->frame->sample_rate, 0, NULL);
        if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
            return -1;
        }
        is->audio_src.channel_layout = dec_channel_layout;
        is->audio_src.channels = af->frame->channels;
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = static_cast<AVSampleFormat>(af->frame->format);
    }
    if (is->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        int out_cout = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256; // 计算所需音频缓冲区的大小,256是额外大小防止缓冲区溢出
        int out_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_cout, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            int  sample_delta = (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate;
            int compensation_distance = wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate;
            if (swr_set_compensation(is->swr_ctx, sample_delta, compensation_distance) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf_size, out_size);
        if (!is->audio_buf1) {
            return AVERROR(ENOMEM);
        }
        len2 = swr_convert(is->swr_ctx, out, out_cout, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_cout) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(is->swr_ctx) < 0) {
                swr_free(&is->swr_ctx);
            }
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.channels *av_get_bytes_per_sample(is->audio_tgt.fmt);
    }else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }
    audio_clock0 = is->audio_clock;
    if (!isnan(af->pts)) {
        is->audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
    }else {
        is->audio_clock = NAN;
    }
    is->audio_clock_serial = af->serial;
    return resampled_data_size;
}

void HHVideoPlayer::update_sample_display(VideoState *is, short *samples, int samples_size) {
    int size, len;
    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
        samples += len;
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }
}


int HHVideoPlayer::get_master_sync_type(VideoState *is) {
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) { // 如果主同步类型是"AV_SYNC_VIDEO_MASTER"，则返回"AV_SYNC_VIDEO_MASTER"，表示视频流作为主时钟。
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) { // 如果主同步类型是"AV_SYNC_AUDIO_MASTER"，则返回"AV_SYNC_AUDIO_MASTER"，表示音频流作为主时钟。
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else { // 如果主同步类型是其他类型，则返回"AV_SYNC_EXTERNAL_CLOCK"，表示使用外部时钟作为主时钟。
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

double HHVideoPlayer::get_master_clock(VideoState *is) {
    double val;
 // 函数通过调用get_master_sync_type(is)函数获取当前的同步类型，并根据同步类型选择使用哪个时钟来获取当前主时钟值
    switch (get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = get_clock(&is->vidclk); // 如果同步类型是AV_SYNC_VIDEO_MASTER，则使用is->vidclk时钟来获取主时钟值；；
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_clock(&is->audclk); // 如果同步类型是AV_SYNC_AUDIO_MASTER，则使用is->audclk时钟
            break;
        default:
            val = get_clock(&is->extclk); // 否则，使用is->extclk时钟。
            break;
    }
    return val;
}

int HHVideoPlayer::synchronize_audio(VideoState *is, int nb_samples) {
    int wanted_nb_samples = nb_samples;
    
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;
        diff = get_clock(&is->audclk) - get_master_clock(is);// 计算音频时钟和主时钟之间的差异
        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                is->audio_diff_avg_count ++;
            }else {
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);
                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));// 调整音频样本数量时允许的最小值
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100)); // 样本数量的最大百分比调整值。
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
            }
        }
    }else {
        is->audio_diff_avg_count = 0;
        is->audio_diff_cum = 0;
    }

    return wanted_nb_samples;
}

