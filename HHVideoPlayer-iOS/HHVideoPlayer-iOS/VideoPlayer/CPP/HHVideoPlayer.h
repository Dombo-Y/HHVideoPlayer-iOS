//
//  HHVideoPlayer.h
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/8.
//

#ifndef HHVideoPlayer_h
#define HHVideoPlayer_h
#include <stdio.h> 
#include <libswresample/swresample.h>
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
    SwrContext *aSwrCtx = nullptr;
    char _filename[512];    /* 文件名 */
    void playerfree();
    void freeAudio();
    void freeVideo();
    void fataError();
    void initVideoState();
    void setState(int state);    //改变状态
    bool initAudioInfo();
    bool initVideoInfo(); 
    int initAudioSwr();
    int initVideoSwr();
    
    void addAudioPkt(AVPacket *pkt);
    void addVideoPkt(AVPacket *pkt);
    int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
    
    int packet_queue_init(PacketQueue *q);
    
    int packet_queue_put(PacketQueue *q, AVPacket *pkt);
    int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
    
    int stream_component_open(VideoState *tis, int stream_index);
    
    void packet_queue_start(PacketQueue *q);
    
    int decoder_start(Decoder *d);
    void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);
    
    int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
//    int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);
//    int audio_thread(void *arg);
    
    void initSwr();
    
    int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
    
//    void event_loop(VideoState *cur_stream);
    void init_clock(Clock *c, int *queue_serial);
    int stream_component_openA(VideoState *tis, int stream_index);
     
//    void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
    int audio_decode_frame(VideoState *is);
    void update_sample_display(VideoState *is, short *samples, int samples_size);
    int synchronize_audio(VideoState *is, int nb_samples);
    void sync_clock_to_slave(Clock *c, Clock *slave);
    double get_clock(Clock *c);
    
    double get_master_clock(VideoState *is);
    int get_master_sync_type(VideoState *is);
    void frame_queue_next(FrameQueue *f);
    
    int initSDL();
    static void sdlAudioCallbackFunc(void *userdata, Uint8 *stream, int len);
    void sdlAudioCallback(Uint8 *stream, int len);
    
    
    int aSwrOutIdx = 0;
    int aSwrOutSize = 0;     //音频重采样后输出的PCM数据大小
    AVFrame *aSwrInFrame = nullptr,*aSwrOutFrame = nullptr;
    AudioSwrSpec aSwrInSpec,aSwrOutSpec;   //音频重采样输入/输出参数
    int decodeAudio();
    
//    static void read_thread(void *arg);
    
    int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);
    void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes);
    int frame_queue_nb_remaining(FrameQueue *f);
    static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
};
#endif /* HHVideoPlayer_hpp */
