//
//  HHVideoPlayer.h
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/8.
//

#ifndef HHVideoPlayer_h
#define HHVideoPlayer_h
#include <stdio.h>
 
#include "videoState.hpp"

class HHVideoPlayer {
public:
     
    HHVideoPlayer();
    ~HHVideoPlayer();
    
    void play();
    void pause();
    void stop();
    bool isPlaying();
    int getState();
    void setFilename(const char *filename);
    int getDuration();
    int getTime();
    void setTime(int time);
    void setVolumn(int volumn);
    int getVolumn();
    void setMute(bool mute);
    bool isMute();
    void readFile();
    void setSelf(void *self);
    void *self; 
    
private:
    VideoState *is; 
    char _filename[512];    /* 文件名 */
    void playerfree();
    void freeAudio();
    void freeVideo();
    void fataError();
    void initVideoState();
    void setState(int state);    //改变状态
    bool initAudioInfo();
    bool initVideoInfo();
    int initDecoder(AVCodecContext **decodeCtx,AVStream **stream,AVMediaType type);
    int initAudioSwr();
    int initVideoSwr();
    
    void addAudioPkt(AVPacket *pkt);
    void addVideoPkt(AVPacket *pkt);
    void packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
    
    int packet_queue_init(PacketQueue *q);
    
    int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
    
    int stream_component_open(VideoState *tis, int stream_index);
    
    void packet_queue_start(PacketQueue *q);
    
    int decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void* arg);
    void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);
    
    int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
    int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);
    int audio_thread(void *arg);
    
    void initSwr();
    
};
#endif /* HHVideoPlayer_hpp */
