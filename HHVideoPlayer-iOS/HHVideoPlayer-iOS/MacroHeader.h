//
//  MacroHeader.h
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/12.
//

#ifndef MacroHeader_h
#define MacroHeader_h

#define SAMPLE_RATE 44100 // 采样率
#define SAMPLE_FORMAT AUDIO_S16LSB// 采样格式
#define SAMPLE_SIZE SDL_AUDIO_BITSIZE(SAMPLE_FORMAT)// 采样大小
#define CHANNELS 2 // 声道数
#define SAMPLES 512 // 音频缓冲区的样本数量
#define SDL_AUDIO_MIN_BUFFER_SIZE 512 // 表示 SDL 音频缓冲区的最小大小
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30 // 表示音频回调的最大次数
#define AV_NOSYNC_THRESHOLD 10.0 // 表示如果同步错误太大，则不会进行音视频同步
#define SAMPLE_CORRECTION_PERCENT_MAX 10 // 表示音频速度变化的最大值。

#define VIDEO_PICTURE_QUEUE_SIZE 3 // 视频帧缓冲队列的大小，即存储待显示的视频帧的数量。
#define SUBPICTURE_QUEUE_SIZE 16 // 字幕缓冲队列的大小，即存储待显示的字幕数据的数量。
#define SAMPLE_QUEUE_SIZE 9 // 音频采样缓冲队列的大小，即存储待播放的音频采样数据的数量。
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

#define MAX_QUEUE_SIZE (15 * 1024 * 1024) //表示音视频队列的最大大小。
#define MIN_FRAMES 25 // 表示播放最少需要的帧数。
#define AUDIO_DIFF_AVG_NB   20 // 表示用于计算平均音视频差异的音视频差异数量。

#define SAMPLE_ARRAY_SIZE (8 * 65536) // 表示音频样本数组的大小


#endif /* MacroHeader_h */
