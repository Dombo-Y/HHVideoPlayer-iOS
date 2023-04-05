//
//  PlayerC_interface.h
//  HHVideoPlayer_FFmpeg
//
//  Created by 尹东博 on 2023/4/5.
//

#ifndef HHObject_C_Interface_h
#define HHObject_C_Interface_h


int playerDoSomethingWith (void *hhObjectInstance, void *parameter);
#pragma mark 解码完成绘制视频帧
void playerDoDraw(void *hhObjectInstance,void *data, uint32_t w, uint32_t h);
#pragma mark 播放器状态改变
void stateChanged(void *hhObjectInstance);
#pragma mark 音视频解码器初始化完毕
void initFinished(void *hhObjectInstance);
#pragma mark 音视频播放音频时间变化
void timeChanged(void *hhObjectInstance);
#pragma mark 音视频播放失败
void playFailed(void *hhObjectInstance);


#endif /* HHObject_C_Interface_h */
//
