//
//  videoState.hpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/7.
//

#ifndef videoState_hpp
#define videoState_hpp

#include <stdio.h>
//#include "HHPrefixHeader.pch"
//#include "HHHeader.h"
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>//格式
#include <libavutil/avutil.h>//工具
#include <libavcodec/avcodec.h>//编码
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>//重采样
#include <libswscale/swscale.h>//像素格式转换

#include "SDL.h"

#ifdef __cplusplus
};
#endif


#define VIDEO_PICTURE_QUEUE_SIZE 3 // 视频帧缓冲队列的大小，即存储待显示的视频帧的数量。
#define SUBPICTURE_QUEUE_SIZE 16 // 字幕缓冲队列的大小，即存储待显示的字幕数据的数量。
#define SAMPLE_QUEUE_SIZE 9 // 音频采样缓冲队列的大小，即存储待播放的音频采样数据的数量。
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

#define MAX_QUEUE_SIZE (15 * 1024 * 1024) //表示音视频队列的最大大小。
#define MIN_FRAMES 25 // 表示播放最少需要的帧数。
#define AUDIO_DIFF_AVG_NB   20 // 表示用于计算平均音视频差异的音视频差异数量。

#define SAMPLE_ARRAY_SIZE (8 * 65536) // 表示音频样本数组的大小

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */ // 表示以音频作为主时钟进行同步，为默认选择。
    AV_SYNC_VIDEO_MASTER, // 表示以视频作为主时钟进行同步。
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */ // 示以外部时钟作为主时钟进行同步。
};

typedef enum  {
    Stopped = 0,
    Playing,
    Paused
}HHVideoState;


static int av_sync_type = AV_SYNC_AUDIO_MASTER;
static int infinite_buffer = -1;
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;
static int decoder_reorder_pts = -1;

typedef struct MyAVPacketList {
    AVPacket pkt;// 类型为AVPacket 的结构体，用于存储音视频数据包
    struct MyAVPacketList *next;
    int serial; //当前视频包的序列号
}MyAVPacketList;// 用于存储AVPacket数据的链表结构体，主要用于PacketQueue的队列实现

typedef struct Frame {
    AVFrame *frame;//指向解码后的视频帧
    int serial ; // 视频帧的序列号，用于与音频帧的序列号进行比较，以确保音视频同步
    double pts; // 视频帧的显示时间戳（Presentation Time Stamp）
    int64_t pos; // 视频帧的字节偏移量，用于seek时跳转到正确的位置
    double duration; // 视频帧的持续时间
    int width;// 视频帧的宽度
    int height;// 视频帧的高度
    int format; // 视频帧的像素格式
    AVRational sar;// 视频帧的采样比例（Sample Aspect Ratio）
    int uploaded; // 标志位，用于指示是否已上传到GPU，如果已经上传到GPU，则可以进行硬件加速渲染
    int flip_v;// 标志位，用于指示是否需要在垂直方向上翻转视频帧，因为有些视频的存储方式与FFmpeg默认方式不同，导致视频帧需要翻转才能正常显示
}Frame;// 用于存储视频帧的信息

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;// 这两个指针用于指向队列中第一个和最后一个AVPacket结构体的节点，以便快速插入和删除数据。
//    AVFifo *pkt_list;
    int nb_packets; // 队列中AVPacket的数量。
    int size;   // 队列中AVPacket的总大小
    int64_t duration;// 队列中的数据包总时长
    int abort_request;// 是否在终止数据包的读取和处理
    int serial;// 用于标识队列的顺序
    SDL_mutex *mutex; // 互斥锁
    SDL_cond *cond;// 信号变量
}PacketQueue; // 用于管理音频和视频帧的队列，该队列用于缓存尚未被处理的音视频帧，以便在适当的时候进行播放

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE]; //用于存储视频帧的数组
    int rindex; // 队列中最早的视频帧的索引
    int windex; // 队列中最新的视频帧的索引
    int size; // 队列中视频帧的数量
    int max_size; // 队列中视频帧的最大数量
    int keep_last; // 标志位，用于指示是否需要保留最后一帧，取值0或1
    int rindex_shown; // 队列中最早显示的视频帧的索引
    SDL_mutex *mutex; // 互斥锁
    SDL_cond *cond; // 条件变量
    PacketQueue *pktq; // 指向存储待显示AVPacket的PacketQueue队列
}FrameQueue; // 用于管理视频帧的队列，该队列用于缓存尚未被显示的视频帧

typedef struct {
    int sampleRate; // 音频采样率
    AVSampleFormat sampleFmt;// 音频样本格式。
    int chLayout; // 音频通道布局。
    int chs;// 音频通道数。
    //每一个样本帧(两个声道(左右声道))的大小
    int bytesPerSampleFrame;// 每个音频样本帧（即一组采样数据）的大小，单位为字节
} AudioSwrSpec;

typedef struct Clock {
    double pts;           /* clock base */// 当前时钟基准时间。
    double pts_drift;     /* clock base minus time at which we updated the clock */// 时钟基准时间减去最后一次更新时的时间
    double last_updated; // 上次更新时钟的时间
    double speed; // 时钟的速度。
    int serial;           /* clock is based on a packet with this serial */ // 时钟基于的数据包的序列号。
    int paused; // 是否暂停了时钟。
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */ // 指向当前数据包队列序列号的指针，用于检测过时的时钟。
} Clock;

typedef struct AudioParams {
    int freq; // 音频采样率，单位为 Hz
    int channels; // 音频通道数，表示音频数据是单声道还是立体声等。
    int64_t channel_layout; // 音频通道布局，表示音频数据各个通道的位置关系。
    enum AVSampleFormat fmt;// 频采样格式，表示采样值的编码方式。
    int frame_size; // 每个音频帧的大小，单位为字节。
    int bytes_per_sec; // 每秒钟的音频数据大小，单位为字节。
} AudioParams;


typedef struct Decoder {
    AVPacket pkt; // 解码器当前处理的AVPacket结构体
    PacketQueue *queue; // 指向存储待解码AVPacket的PacketQueue队列
    AVCodecContext *avctx; // 解码器的上下文信息，包括解码器本身的相关信息和解码后的输出信息
    int pkt_serial; //指示解码器处理的AVPacket结构体的序列号
    int finished; // 标志位，用于指示解码是否完成
    int packet_pending; // 标志位，用于指示是否有未处理的AVPacket，取值为0或1
    SDL_cond *empty_queue_cond; // 用于等待和通知PacketQueue队列是否为空的条件变量，用于线程之间的同步
    int64_t start_pts; // 解码器第一帧的PTS（Presentation Time Stamp），开始解码的时间戳
    AVRational start_pts_tb; // 解码器第一帧的PTS时间基（Time Base），开始解码的时间戳对应的时间基（timebase）
    int64_t next_pts; // 下一帧数据的时间戳
    AVRational next_pts_tb; //  下一帧数据的时间戳对应的时间基
    SDL_Thread *decoder_tid; // 解码器所在线程的指针
} Decoder; // 用于解码音频或视频帧
 
typedef struct FrameData {
    int64_t pkt_pos;
} FrameData;

typedef struct VideoState {
    SDL_Thread *read_tid;//
    AVInputFormat *iformat ;//输入的媒体格式
    AVFormatContext *ic; //输入的媒体文件的上下文信息
    
    AVCodecContext *audioCodecCtx;
    AVCodecContext *videoCodecCtx;
    AVCodec *aCodec;
    AVCodec *vCodec;
    
    int realtime; // 是否以实时模式进行播放
    
    int abort_request; // 是否终止媒体播放的请求
    int paused; // 是否暂停媒体播放
    int last_paused; //  上一次暂停的时间
    int read_pause_return; // 读取暂停的返回值
    
    int seek_req; // 是否需要跳转到媒体文件的某个位置
    int seek_flags; // 跳转的标识符
    
    int64_t seek_pos; // 跳转的绝对位置
    int64_t seek_rel; // 相对跳转位置
     
    int16_t sample_array[SAMPLE_ARRAY_SIZE]; // 存储音频样本的数组
    int sample_array_index; // 音频样本数组中当前的索引值
    int audio_stream; // 音频流的索引号
    int video_stream;// 视频流的索引号
    char *filename;// 文件名称
    int last_video_stream, last_audio_stream, last_subtitle_stream;
    SDL_cond *continue_read_thread;// 读取线程的条件变量
    
    FrameQueue pictq; // 解码后的视频帧队列
    FrameQueue sampq; // 解码后的音频帧队列
     
    PacketQueue videoq; // 解码前的视频帧队列
    PacketQueue audioq; // 解码前的音频帧队列
    
    Clock audclk;//音频时钟
    Clock vidclk;//视频时钟
    Clock extclk;//外部时钟
    
    Decoder auddec;// 音频解码器
    Decoder viddec; //  视频解码器
    
    AVStream *audio_st; // 音频流的AVStream指针
    AVStream *video_st; // 视频流的上下文
     
    int audio_clock_serial; // 音频时钟的序列号
    double audio_clock; // 音频时钟
    int eof; // : 判断是否已到达文件末尾
    int av_sync_type; // 音视频同步类型
    int step; // 视频帧步长
    
    double max_frame_duration; // 视频帧的最大持续时间，超过了的设定值，应用程序应该将其忽略或者重新计算时间戳，以便更好地控制帧率和播放速度。
        
    
    struct SwrContext *swr_ctx;
    struct AudioParams audio_tgt;// 音频解码后的参数
    struct AudioParams audio_src; // 音频解码前的参数
    
    int audio_hw_buf_size; // 音频硬件缓冲区大小
    unsigned int audio_buf_size; /* in bytes */ // 音频缓冲区大小
    unsigned int audio_buf1_size; // 音频缓冲区1的大小
    int audio_buf_index; /* in bytes */// 音频缓冲区的索引
    int audio_write_buf_size; // 音频写入缓冲区的大小
    
    double audio_diff_cum; // 音频差异的累计值
    double audio_diff_avg_coef; // 音频差异的平均系数
    double audio_diff_threshold; // 音频差异的阈值
    int audio_diff_avg_count; // 音频差异的平均计数
    
    uint8_t *audio_buf; // 音频缓冲区
    uint8_t *audio_buf1; // 音频缓冲区1
    
    bool  haveAudio; // 有音频
    bool  haveVideo; // 有视频
    
    int64_t start_pts; // 开始解码的时间戳
    AVRational start_pts_tb; // 开始解码的时间戳对应的时间基（timebase）
    
    HHVideoState state = Stopped;
    int seekTime = -1;
       
    
    double frame_timer; // 视频帧计时器
    double frame_last_returned_time; // 上一次返回帧的时间
    double frame_last_filter_delay; // 上一次滤镜延迟的时间
    
    int frame_drops_early; // 丢弃的早期帧数
    int frame_drops_late; // 丢弃的后期帧数
    
//    SwrContext *_aSwrCtx = nullptr;    //音频重采样上下文
//    AudioSwrSpec _aSwrInSpec,_aSwrOutSpec;   //音频重采样输入/输出参数
//    struct AudioParams audio_src; // 音频解码前的参数
//    struct AudioParams audio_tgt;// 音频解码后的参数
    AVFrame *_aSwrInFrame = nullptr,*_aSwrOutFrame = nullptr;   //存放解码后的音频重采样输入/输出数据
    
}VideoState;

#endif /* videoState_hpp */
