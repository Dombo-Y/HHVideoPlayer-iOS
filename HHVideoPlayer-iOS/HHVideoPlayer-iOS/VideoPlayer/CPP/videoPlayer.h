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
    //播放器状态
    typedef enum {
        Stopped = 0,
        Playing,
        Paused
    } State;
    //音量
    typedef enum {
        Min = 0,
        Max = 100
    } Volumn;

    //视频frame参数
    typedef struct {
        int width;
        int height;
        AVPixelFormat pixFmt;
        int size;
    } VideoSwsSpec;

    VideoPlayer();
    ~VideoPlayer();
    /* 播放 */
    void play();
    /* 暂停 */
    void pause();
    /* 停止 */
    void stop();
    /* 是否正在播放中 */
    bool isPlaying();
    /* 获取当前状态 */
    State getState();
    /* 设置文件名 */
    void setFilename(const char *filename);
    /* 获取总时长(单位是秒，1秒 = 10^3毫秒 = 10^6微妙) */
    int getDuration();
    /* 当前播放时刻 秒 */
    int getTime();
    /* 设置当前播放时刻(单位是秒) */
    void setTime(int time);
    /* 设置音量 */
    void setVolumn(int volumn);
    /* 获取当前音量 */
    int getVolume();
    /* 设置静音 */
    void setMute(bool mute);
    bool isMute();
    /**--------------------------------------*/
    /* 读取文件 */
    void readFile();
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

    //解码上下文
    AVCodecContext *_aDecodeCtx = nullptr;
    //音频流
    AVStream *_aStream = nullptr;
    //存放音频包的列表  ---- 跟随VideoPlayer生而生死而死，用对象不用指针
    std::list<AVPacket> _aPktList;
    //音频包列表的锁 ---- 跟随VideoPlayer生而生死而死，用对象不用指针
    CondMutex _aMutex;
    //音频重采样上下文
    SwrContext *_aSwrCtx = nullptr;
    //音频重采样输入/输出参数
    AudioSwrSpec _aSwrInSpec,_aSwrOutSpec;
    //存放解码后的音频重采样输入/输出数据
    AVFrame *_aSwrInFrame = nullptr,*_aSwrOutFrame = nullptr;
    //从哪个位置开始取出PCM数据填充到SDL的音频缓冲区，应对复杂情况，PCM大于缓冲区，需要二次填充记录已经填充的PCM的索引位置
    int _aSwrOutIdx = 0;
    //音频重采样后输出的PCM数据大小
    int _aSwrOutSize = 0;
    //音频时钟
    double _aTime = 0;
    //音频资源是否可以释放
    bool _aCanFree = false;
    //是否有音频流
    bool _hasAudio = false;
    /* 外面设置的当前播放时刻(用于完成seek功能) 不要为0，有可能回退到0，但不可能回退到-1，-1表示没有人做seek操作 */
    int _aSeekTime = -1;

    //初始化音频信息
    int initAudioInfo();
    //初始化SDL
    int initSDL();
    //初始化音频重采样
    int initSwr();
    //添加数据包到音频列表中
    void addAudioPkt(AVPacket &pkt);
    //清空音频数据包列表
    void clearAudioPktList();
    //SDL填充缓冲区的回调函数
    static void sdlAudioCallbackFunc(void *userdata,Uint8 *stream,int len);
    //SDL填充缓冲区的回调函数
    void sdlAudioCallback(Uint8 *stream,int len);
    //音频解码
    int decodeAudio();

    /************** 视频相关 *************/
    //解码上下文
    AVCodecContext *_vDecodeCtx = nullptr;
    //视频流
    AVStream *_vStream = nullptr;
    //存放解码后的视频像素格式输入/输出数据
    AVFrame *_vSwsInFrame = nullptr,*_vSwsOutFrame = nullptr;
    //像素格式转换上下文
    SwsContext *_vSwsCtx = nullptr;
    //输出frame参数
    VideoSwsSpec _vSwsOutSpec;
    //存放视频包的列表 ---- 跟随VideoPlayer生而生死而死，用对象不用指针
    std::list<AVPacket> _vPktList;
    //视频包列表的锁 ---- 跟随VideoPlayer生而生死而死，用对象不用指针
    CondMutex _vMutex;
    //初始化视频信息
    int initVideoInfo();
    //初始化视频像素格式转换
    int initSws();
    //添加数据包到视频列表中
    void addVideoPkt(AVPacket &pkt);
    //清空视频数据包列表
    void clearVideoPktList();
    //解码视频
    void decodeVideo();
    //视频时钟
    double _vTime = 0;
    //视频资源是否可以释放
    bool _vCanFree = false;
    //是否有视频流
    bool _hasVideo = false;
    /* 外面设置的当前播放时刻(用于完成seek功能) 不要为0，有可能回退到0，但不可能回退到-1，-1表示没有人做seek操作 */
    int _vSeekTime = -1;

    /***** 其他 *****/
    //解封装上下文
    AVFormatContext *_fmtCtx = nullptr;
    int initDecoder(AVCodecContext **decodeCtx,AVStream **stream,AVMediaType type);
    //fmtCtx是否可以释放
    bool _fmtCtxCanFree = false;
    //音量
    int _volumn = Max;
    //静音
    bool _mute = false;
    //保留播放器当前状态
    State _state = Stopped;
    //改变状态
    void setState(State state);
    /* 文件名 */
    char _filename[512];
    /* 外面设置的当前播放时刻(用于完成seek功能) 不要为0，有可能回退到0，但不可能回退到-1，-1表示没有人做seek操作 */
    int _seekTime = -1;
    //释放资源
    void playerfree();
    void freeAudio();
    void freeVideo();
    void fataError();
    
};
#endif /* videoPlayer_hpp */
