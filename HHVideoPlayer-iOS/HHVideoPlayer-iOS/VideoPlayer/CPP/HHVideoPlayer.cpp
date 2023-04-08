//
//  HHVideoPlayer.cpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/8.
//

#include "HHVideoPlayer.h"
#include <thread>
#include<iostream>
 
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif
    
#include <libavcodec/avcodec.h>
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
    
#include "SDL_main.h"
#include <SDL_thread.h>
    
    
#ifdef __cplusplus
};
#endif

#define SAMPLE_RATE 44100 // 采样率
#define SAMPLE_FORMAT AUDIO_S16LSB// 采样格式
#define SAMPLE_SIZE SDL_AUDIO_BITSIZE(SAMPLE_FORMAT)// 采样大小
#define CHANNELS 2 // 声道数
#define SAMPLES 512 // 音频缓冲区的样本数量

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
//    ret = packet_queue_init(&iv->videoq);
//    ret = packet_queue_init(&iv->audioq);
   
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
    
}

HHVideoPlayer::~HHVideoPlayer() {
    printf("析构函数～～～～");
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
    
    std::thread([this](){
//        decodeVideo();
    }).detach();
    
    AVFrame *inFrame = av_frame_alloc();
    AVFrame *outFrame = av_frame_alloc();
    AVPacket aPacket;
    AVPacket vPacket;
    int index = 0;
    while (is->state != Stopped) {
        int vSize = is->videoq.size;
        int aSize = is->audioq.size;
        if (vSize + aSize  > 1000) {
//            SDL_Delay(10);
            continue;; // 缓存足够大了就缓存
        }
          
        av_format_inject_global_side_data(is->ic);
        ret = av_read_frame(is->ic, &aPacket);
        index ++;
        if (ret == 0) {
            if (aPacket.stream_index == is->audio_stream) {
                addAudioPkt(aPacket);
                cout << &aPacket << "音频音频音频音频音频" << index  << endl;
            }else if (aPacket.stream_index == is->video_stream) {
                addVideoPkt(aPacket);
                cout << &aPacket << "视频视频视频视频" << endl;
            }else {
//                av_packet_unref(aPacket);
            }
        }else if (ret == AVERROR_EOF) {
            if (vSize == 0 && aSize == 0) {
                 //释放 ctx？
            }
        } else {
            cout << "aaaaaa" << endl ;
            continue;
        }
    }
//    if() {
//        读取到文件尾部了
//        stop()
//    }
}

void HHVideoPlayer::addAudioPkt(AVPacket &pkt) {
    
}

void HHVideoPlayer::addVideoPkt(AVPacket &pkt) {
    
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
    
    SwrContext *aSwrCtx = nullptr;    //音频重
//    aSwrCtx = swr_alloc_set_opts(nullptr, out_chLayout, out_samplefmt, outSampleRate, in_channel_layout, in_sp_fmt, in_sample_rate, 0, nullptr);
    
    ret = swr_init(aSwrCtx);
    av_frame_alloc();
    
    AVFrame *outFrame = av_frame_alloc(); //输出frame
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

