//
//  videoPlayer.hpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/5.
//

#ifndef videoPlayer_hpp
#define videoPlayer_hpp


#include "list"
#include "condmutex.h"
#include <iostream>
#include "videoState.hpp"
using namespace std;
#include "PlayerCInterface.h"

extern "C" {
//格式
#include <libavformat/avformat.h>
//工具
#include <libavutil/avutil.h>
//编码
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
//重采样
#include <libswresample/swresample.h>
//像素格式转换
#include <libswscale/swscale.h>
}

#define ERROR_BUF \
    char errbuf[1024];\
    av_strerror(ret,errbuf,sizeof(errbuf));

#define CODE(func,code) \
    if(ret < 0) { \
    ERROR_BUF;\
    printf("%s error %s",#func,errbuf);\
    cout << #func << "error" << errbuf;\
    code;\
    }

#define END(func) CODE(func,fataError();return;)

#define RET(func) CODE(func,return ret;)

#define CONTINUE(func) CODE(func,continue;)

#define BREAK(func) CODE(func,break;)
/*
 * 预处理视频，不负责显示、渲染视频
 */
class VideoPlayer
{
public:
    typedef enum { //播放器状态
        Stopped = 0,
        Playing,
        Paused
    } State;
    
    typedef enum { //音量
        Min = 0,
        Max = 100
    } Volumn;

   
    typedef struct {  //视频frame参数
        int width;
        int height;
        AVPixelFormat pixFmt;
        int size;
    } VideoSwsSpec;

    VideoPlayer();
    ~VideoPlayer();
   
    void play();  /* 播放 */
    void pause(); /* 暂停 */
    void stop(); /* 停止 */
    bool isPlaying(); /* 是否正在播放中 */
    State getState(); /* 获取当前状态 */
    void setFilename(const char *filename);/* 设置文件名 */
    int getDuration(); /* 获取总时长(单位是秒，1秒 = 10^3毫秒 = 10^6微妙) */
    int getTime();/* 当前播放时刻 秒 */
    void setTime(int time);   /* 设置当前播放时刻(单位是秒) */
    void setVolumn(int volumn);/* 设置音量 */
    int getVolume(); /* 获取当前音量 */
    void setMute(bool mute); /* 设置静音 */
    bool isMute();
    
    /**--------------------------------------*/
    void readFile(); /* 读取文件 */
    void setSelf(void *self);
    int someMethod (void *objectiveCObject, void *aParameter);
    void *self;
    
    
private:
    /************** 音频相关 **************/
    typedef struct {
        int sampleRate;
        AVSampleFormat sampleFmt;
        int chLayout;
        int chs;
        //每一个样本帧(两个声道(左右声道))的大小
        int bytesPerSampleFrame;
    } AudioSwrSpec;
 
    AVCodecContext *_aDecodeCtx = nullptr;//解码上下文
  
    AVStream *_aStream = nullptr;   //音频流
    std::list<AVPacket> _aPktList;  //存放音频包的列表  ---- 跟随VideoPlayer生而生死而死，用对象不用指针
    CondMutex _aMutex;    //音频包列表的锁 ---- 跟随VideoPlayer生而生死而死，用对象不用指针
    SwrContext *_aSwrCtx = nullptr;    //音频重采样上下文
    AudioSwrSpec _aSwrInSpec,_aSwrOutSpec;   //音频重采样输入/输出参数
    AVFrame *_aSwrInFrame = nullptr,*_aSwrOutFrame = nullptr;   //存放解码后的音频重采样输入/输出数据
    //从哪个位置开始取出PCM数据填充到SDL的音频缓冲区，应对复杂情况，PCM大于缓冲区，需要二次填充记录已经填充的PCM的索引位置
    
    int _aSwrOutIdx = 0;
    int _aSwrOutSize = 0;     //音频重采样后输出的PCM数据大小

    double _aTime = 0;    //音频时钟

    bool _aCanFree = false;    //音频资源是否可以释放

    bool _hasAudio = false;    //是否有音频流
   int _aSeekTime = -1;  /* 外面设置的当前播放时刻(用于完成seek功能) 不要为0，有可能回退到0，但不可能回退到-1，-1表示没有人做seek操作 */


    int initAudioInfo();    //初始化音频信息
    int initSDL();    //初始化SDL
    int initSwr();    //初始化音频重采样
    void addAudioPkt(AVPacket &pkt);    //添加数据包到音频列表中
    void clearAudioPktList();    //清空音频数据包列表
    static void sdlAudioCallbackFunc(void *userdata,Uint8 *stream,int len);    //SDL填充缓冲区的回调函数
    void sdlAudioCallback(Uint8 *stream,int len);    //SDL填充缓冲区的回调函数
    int decodeAudio();   //音频解码

    /************** 视频相关 *************/
    AVCodecContext *_vDecodeCtx = nullptr; //解码上下文

    AVStream *_vStream = nullptr;    //视频流
  
    AVFrame *_vSwsInFrame = nullptr,*_vSwsOutFrame = nullptr;  //存放解码后的视频像素格式输入/输出数据
    SwsContext *_vSwsCtx = nullptr;   //像素格式转换上下文
    VideoSwsSpec _vSwsOutSpec;    //输出frame参数
    std::list<AVPacket> _vPktList;  //存放视频包的列表 ---- 跟随VideoPlayer生而生死而死，用对象不用指针
  
    CondMutex _vMutex;  //视频包列表的锁 ---- 跟随VideoPlayer生而生死而死，用对象不用指针

    int initVideoInfo();    //初始化视频信息
    int initSws();    //初始化视频像素格式转换
    void addVideoPkt(AVPacket &pkt);    //添加数据包到视频列表中
    void clearVideoPktList();    //清空视频数据包列表
    void decodeVideo();    //解码视频
    double _vTime = 0;    //视频时钟

    bool _vCanFree = false;    //视频资源是否可以释放
    bool _hasVideo = false;    //是否有视频流
    /* 外面设置的当前播放时刻(用于完成seek功能) 不要为0，有可能回退到0，但不可能回退到-1，-1表示没有人做seek操作 */
    int _vSeekTime = -1;

    /***** 其他 *****/
    //解封装上下文
    AVFormatContext *_fmtCtx = nullptr;
    int initDecoder(AVCodecContext **decodeCtx,AVStream **stream,AVMediaType type);
   
    bool _fmtCtxCanFree = false; //fmtCtx是否可以释放
    int _volumn = Max;   //音量
    bool _mute = false;    //静音
    State _state = Stopped;    //保留播放器当前状态
    void setState(State state);    //改变状态
    char _filename[512];    /* 文件名 */
    /* 外面设置的当前播放时刻(用于完成seek功能) 不要为0，有可能回退到0，但不可能回退到-1，-1表示没有人做seek操作 */
    int _seekTime = -1;

    void playerfree();  //释放资源
    void freeAudio();
    void freeVideo();
    void fataError();
    void initVideoState();
    
    VideoState *is;// 用于存储音视频文件的信息状态用
};
#endif /* videoPlayer_hpp */
