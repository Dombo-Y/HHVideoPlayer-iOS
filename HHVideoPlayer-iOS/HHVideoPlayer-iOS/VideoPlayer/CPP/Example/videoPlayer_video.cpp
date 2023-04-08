//
//  videoPlayer_video.cpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/5.
//

#include "videoPlayer.h"
#include "PlayerCInterface.h"

extern "C" {
 #include <libavutil/imgutils.h>
}


int VideoPlayer::initVideoInfo(){ //初始化视频信息
    int ret = initDecoder(&_vDecodeCtx,&_vStream,AVMEDIA_TYPE_VIDEO);
    RET(initDecoder);

    ret = initSws();  //初始化视频像素格式转换
    RET(initSws);

    return 0;
}

int VideoPlayer::initSws(){ //初始化视频像素格式转换
    _vSwsInFrame = av_frame_alloc();   //初始化输入Frame
    if(!_vSwsInFrame){
        cout << "av_frame_alloc error" << endl;
        return -1;
    }
    return 0;
}

void VideoPlayer::addVideoPkt(AVPacket &pkt){
    _vMutex.lock();
    _vPktList.push_back(pkt);
    _vMutex.signal();
    _vMutex.unlock();
}

void VideoPlayer::clearVideoPktList(){
    _vMutex.lock();
    for(AVPacket &pkt:_vPktList){    //取出list，前面加*
        av_packet_unref(&pkt);
    }
    _vPktList.clear();
    _vMutex.unlock();
}

void VideoPlayer::freeVideo(){
    _vTime = 0;
    _vCanFree = false;
    clearVideoPktList();
    avcodec_free_context(&_vDecodeCtx);
    av_frame_free(&_vSwsInFrame);
    if(_vSwsOutFrame){
        av_freep(&_vSwsOutFrame->data[0]);
        av_frame_free(&_vSwsOutFrame);
    }
    sws_freeContext(_vSwsCtx);
    _vSwsCtx = nullptr;
    _vStream = nullptr;
    _vSeekTime = -1;
}

void VideoPlayer::decodeVideo(){
    while (true) {
        if(_state == Paused && _vSeekTime == -1) continue;  //如果是暂停状态,并且没有seek操作，暂停状态也能seek
        //如果是停止状态，会调用free，就不用再去解码，重采样，渲染，导致访问释放了的内存空间，会闪退
        if(_state == Stopped){
            _vCanFree = true;
            break;
        }
        _vMutex.lock();
        if(_vPktList.empty()){
            _vMutex.unlock();
            continue;
        }
        
        AVPacket pkt = _vPktList.front();//取出头部的视频包
        _vPktList.pop_front();
        _vMutex.unlock();
 
        int ret = avcodec_send_packet(_vDecodeCtx,&pkt);  //发送数据到解码器

       
        if(pkt.dts != AV_NOPTS_VALUE){  //视频时钟 视频用dts，音频用pts
//            cout << "视频时间基分子:" << _vStream->time_base.num << "---视频时间基分母:" << _vStream->time_base.den << endl;
            _vTime = av_q2d(_vStream->time_base) * pkt.pts;
//            cout << "当前视频时间"<< _vTime << "seek时间" << _vSeekTime << endl;
        }
 
        av_packet_unref(&pkt); //释放pkt 
        CONTINUE(avcodec_send_packet);
        while (true) {  //获取解码后的数据
            ret = avcodec_receive_frame(_vDecodeCtx,_vSwsInFrame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                break;//结束本次循环，重新从_vPktList取出包进行解码
            }else BREAK(avcodec_receive_frame);

            //一定要在解码成功后，再进行下面的判断,防止seek时，到达的是p帧，但前面的I帧已经被释放了，无法参考，这一帧的解码就会出现撕裂现象
            //发现视频的时间是早于seekTime的，就丢弃，防止到seekTime的位置闪现
            if(_vSeekTime >= 0){
                if(_vTime < _vSeekTime){
                    continue;
                }else{
                    _vSeekTime = -1;
                }
            }
            //OPENGL渲染
            char *buf = (char *)malloc(_vSwsInFrame->width * _vSwsInFrame->height * 3 / 2);
            AVPicture *pict;
            int w, h;
            char *y, *u, *v;
            pict = (AVPicture *)_vSwsInFrame;//这里的frame就是解码出来的AVFrame
            w = _vSwsInFrame->width;
            h = _vSwsInFrame->height;
            y = buf;
            u = y + w * h;
            v = u + w * h / 4;
            for (int i=0; i<h; i++)
                memcpy(y + w * i, pict->data[0] + pict->linesize[0] * i, w);
            for (int i=0; i<h/2; i++)
                memcpy(u + w / 2 * i, pict->data[1] + pict->linesize[1] * i, w / 2);
            for (int i=0; i<h/2; i++)
                memcpy(v + w / 2 * i, pict->data[2] + pict->linesize[2] * i, w / 2);
            if(_hasAudio){//有音频
                //如果视频包多早解码出来，就要等待对应的音频时钟到达
                //有可能点击停止的时候，正在循环里面，停止后sdl free掉了，就不会再从音频list中取出包，_aClock就不会增大，下面while就死循环了，一直出不来，所以加Playing判断
                printf("vTime=%lf, aTime=%lf, vTime-aTime=%lf\n", _vTime, _aTime, _vTime - _aTime);
                while(_vTime > _aTime && _state == Playing){//音视频同步

                }
            }else{
                //TODO 没有音频的情况
            }
            playerDoDraw(self,buf,_vSwsInFrame->width,_vSwsInFrame->height);
            
            //TODO ---- 啥时候释放 若立即释放 会崩溃 原因是渲染并没有那么快，OPENGL还没有渲染完毕，但是这块内存已经被free掉了
            //放到OPGLES glview中等待一帧渲染完毕后，再释放，此处不能释放
//            free(buf);
//            cout << "渲染了一帧" << _vTime << "音频时间" << _aTime << endl;
            //子线程把这一块数据_vSwsOutFrame->data[0]直接发送到主线程，给widget里面的image里面的bits指针指向去绘制图片，主线程也会访问这一块内存数据，子线程也会访问，有可能子线程正往里面写着，主线程就拿去用了，会导致数据错乱，崩溃
            //将像素转换后的图片数据拷贝一份出来,OPENGL渲染是一样的道理，需要先保存新的一份字节数据
        }
    }
}
