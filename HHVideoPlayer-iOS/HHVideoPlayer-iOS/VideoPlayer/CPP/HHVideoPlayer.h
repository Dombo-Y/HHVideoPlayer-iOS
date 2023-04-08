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
    
     
};
#endif /* HHVideoPlayer_hpp */
