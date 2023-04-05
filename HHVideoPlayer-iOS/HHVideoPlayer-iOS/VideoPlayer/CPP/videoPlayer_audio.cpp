//
//  videoPlayer_audio.cpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/5.
//

#include "videoPlayer.h"

#include "PlayerCInterface.h"

/* 一些宏定义 */
// 采样率
#define SAMPLE_RATE 44100
// 采样格式
#define SAMPLE_FORMAT AUDIO_S16LSB
// 采样大小
#define SAMPLE_SIZE SDL_AUDIO_BITSIZE(SAMPLE_FORMAT)
// 声道数
#define CHANNELS 2
// 音频缓冲区的样本数量
#define SAMPLES 512
//初始化音频信息
int VideoPlayer::initAudioInfo(){
    /*
     * 命令行生成yuv和pcm  ffmpeg自带音频解码器默认输出fltp格式pcm，libfdk_aac默认输出s16le
       ffmpeg -c:v h264 -c:a libfdk_aac -i in.mp4 cmd_out.yuv -f s16le cmd_out.pcm
       ffmpeg自带解码器输出pcm
       ffmpeg -c:a aac -i in.mp4  -f s16le cmd_out_aac.pcm
     */
    //根据TYPE寻找最合适的流信息
    //初始化解码器
    int ret = initDecoder(&_aDecodeCtx,&_aStream,AVMEDIA_TYPE_AUDIO);
    RET(initDecoder);

    //初始化音频重采样
    ret = initSwr();
    RET(initSwr);
    //初始化SDL
    ret = initSDL();
    RET(initSDL);

    return 0;
}
int VideoPlayer::initSwr(){
    //重采样输入参数
    _aSwrInSpec.sampleRate = _aDecodeCtx->sample_rate;
    _aSwrInSpec.sampleFmt = _aDecodeCtx->sample_fmt;
    _aSwrInSpec.chLayout = _aDecodeCtx->channel_layout;
    _aSwrInSpec.chs = _aDecodeCtx->channels;

    //重采样输出参数
    _aSwrOutSpec.sampleRate = SAMPLE_RATE;
    _aSwrOutSpec.sampleFmt = AV_SAMPLE_FMT_S16;
    _aSwrOutSpec.chLayout = AV_CH_LAYOUT_STEREO;
    _aSwrOutSpec.chs = av_get_channel_layout_nb_channels(_aSwrOutSpec.chLayout);
    _aSwrOutSpec.bytesPerSampleFrame = _aSwrOutSpec.chs * av_get_bytes_per_sample(_aSwrOutSpec.sampleFmt);
    //创建重采样上下文
    _aSwrCtx = swr_alloc_set_opts(nullptr,
                                  // 输出参数
                                  _aSwrOutSpec.chLayout,
                                  _aSwrOutSpec.sampleFmt,
                                  _aSwrOutSpec.sampleRate,
                                  // 输入参数
                                  _aSwrInSpec.chLayout,
                                  _aSwrInSpec.sampleFmt,
                                  _aSwrInSpec.sampleRate,
                                  0, nullptr);
    if (!_aSwrCtx) {
        cout << "swr_alloc_set_opts error" << endl;
        return -1;
    }
    //初始化重采样上下文
    int ret = swr_init(_aSwrCtx);
    RET(swr_init);

    //初始化输入Frame
    _aSwrInFrame = av_frame_alloc();
    if(!_aSwrInFrame){
        cout << "av_frame_alloc error" << endl;
        return -1;
    }

    //初始化输出Frame
    _aSwrOutFrame = av_frame_alloc();
    if(!_aSwrOutFrame){
        cout << "av_frame_alloc error" << endl;
        return -1;
    }
//    cout << "_aSwrOutFrame初始化之前" << _aSwrOutFrame->data[0] << endl;
    //初始化重采样的输出_aSwrOutFrame的data[0]空间，防止重采样的时候，出现采样后的数据不知道放哪儿，
    //初始化前为空指针，提前分配一个足够大的内存空间4096个样本，一般的样本大小为1024
    ret = av_samples_alloc(_aSwrOutFrame->data,_aSwrOutFrame->linesize,_aSwrOutSpec.chs,4096,_aSwrOutSpec.sampleFmt,1);
//    cout << "_aSwrOutFrame初始化之后" << _aSwrOutFrame->data[0] << endl;
    RET(av_samples_alloc);

    return 0;
}
int VideoPlayer::initSDL(){
    //这一句不加音频也可以播放
    SDL_SetMainReady();
    // 音频参数
    SDL_AudioSpec spec;
    // 采样率
    spec.freq = _aSwrOutSpec.sampleRate;
    // 采样格式（s16le）
    spec.format = SAMPLE_FORMAT;
    // 声道数
    spec.channels = _aSwrOutSpec.chs;
    // 音频缓冲区的样本数量（这个值必须是2的幂）
    spec.samples = SAMPLES;
    // 回调
    spec.callback = sdlAudioCallbackFunc;
    // 传递给回调的参数,传递this到类方法中
    spec.userdata = this;
    // 打开音频设备
    if (SDL_OpenAudio(&spec, nullptr)) {
        cout << "SDL_OpenAudio Error " << SDL_GetError() << endl;
        return -1;
    }
    return 0;
}
void VideoPlayer::sdlAudioCallbackFunc(void *userdata, Uint8 *stream, int len){
    //回调函数在SDL开辟的子线程执行
    VideoPlayer *player = (VideoPlayer *)userdata;
    player->sdlAudioCallback(stream,len);
}
/*
 * 上面的pkt虽然只在栈里面创建了一个，但是下面pkt添加到list的时候调用了pkt拷贝构造函数，里面添加的是一个一模一样的全新的pkt
 * AVPacket pkt = _aPktList.front()取出的时候也调用了拷贝构造函数，拷贝了一个一模一样的pkt对象，所以_aPktList.pop_front()执行后删除pkt后并不会影响这个全新的pkt
*/
void VideoPlayer::addAudioPkt(AVPacket &pkt){
    _aMutex.lock();
    _aPktList.push_back(pkt);
    _aMutex.signal();
    _aMutex.unlock();
}
void VideoPlayer::clearAudioPktList(){
    _aMutex.lock();
    //取出list，前面加*
    for(AVPacket &pkt:_aPktList){
        av_packet_unref(&pkt);
    }
    _aPktList.clear();
    _aMutex.unlock();
}
void VideoPlayer::freeAudio(){
    _aTime = 0;
    _aSwrOutIdx = 0;
    _aSwrOutSize = 0;
    _aStream = nullptr;
    _aCanFree = false;
    _aSeekTime = -1;

    clearAudioPktList();
    avcodec_free_context(&_aDecodeCtx);
    av_frame_free(&_aSwrInFrame);
    swr_free(&_aSwrCtx);
    if(_aSwrOutFrame){
        av_freep(&_aSwrOutFrame->data[0]);
        av_frame_free(&_aSwrOutFrame);
    }
    //停止播放
    SDL_PauseAudio(1);
    SDL_CloseAudio();
}
void VideoPlayer::sdlAudioCallback(Uint8 *stream, int len){
   //SDL缓冲区找解码器拉取符合播放格式的音频PCM流stream
   //清零(静音)
    SDL_memset(stream,0,len);
   //len:SDL音频缓冲区剩余的大小(音频缓冲区还未填充的大小)
    while (len > 0) {
        if(_state == Paused) break;
        if(_state == Stopped){
            _aCanFree = true;
             break;
        }
        //说明当前PCM的数据已经全部拷贝到SDL的音频缓冲区了
        //需要解码下一个pkt，获取新的PCM数据
        if(_aSwrOutIdx >= _aSwrOutSize){
            //全新PCM的数据大小
            _aSwrOutSize = decodeAudio();
//            cout << "_aSwrOutSize" << _aSwrOutSize << endl;
            //索引清0
            _aSwrOutIdx = 0;
//            cout << "解码一次的数据大小" << _aSwrOutSize << endl;
            //没有解码出PCM数据那就静音处理
            if(_aSwrOutSize <= 0){
               //出错了，或者还没有解码出PCM数据，假定1024个字节静音处理
                //假定1024个字节
                _aSwrOutSize = 1024;
                //给PCM填充0(静音)
                memset(_aSwrOutFrame->data[0],0,_aSwrOutSize);
            }
        }
        //本次需要填充到stream中的PCM数据大小
        int fillLen = _aSwrOutSize - _aSwrOutIdx;
        fillLen = std::min(fillLen,len);
        //获取音量
        int volumn = _mute ? 0:(_volumn * 1.0/Max) * SDL_MIX_MAXVOLUME;
        //将一个pkt包解码后的pcm数据填充到SDL的音频缓冲区
        SDL_MixAudio(stream,_aSwrOutFrame->data[0]+_aSwrOutIdx,fillLen,volumn);
        //移动偏移量
        len -= fillLen;
        stream += fillLen;
        _aSwrOutIdx += fillLen;
    }
}
int VideoPlayer::decodeAudio(){
    _aMutex.lock();
    //_aPktList中如果是空的，就进入等待，等待_aPktList中新加入解封装后的pkt发送信号signal通知到这儿，
    //有可能解封装很快就都解完成了，后面都没有新的pkt，也不会发送信号了,就会一直在这儿等
    if(_aPktList.empty()){
        cout << "_aPktList为空" << _aPktList.size() << endl;
        _aMutex.unlock();
        return 0;
    }
    //取出list中的头部pkt
    AVPacket &pkt = _aPktList.front();

    //音频包应该在多少秒播放
    if(pkt.pts != AV_NOPTS_VALUE){
      _aTime = av_q2d(_aStream->time_base) * pkt.pts;
      //通知外界:播放时间发生了改变
        timeChanged(self);
    }
    //如果是视频，不能在这个位置判断(不能提前释放pkt,不然会导致B帧、P帧解码失败，画面撕裂)
    //发现音频的时间是早于seekTime的，就丢弃，防止到seekTime的位置闪现
    if(_aSeekTime >= 0){
        if(_aTime < _aSeekTime){
            //释放pkt
            av_packet_unref(&pkt);
            _aPktList.pop_front();
             _aMutex.unlock();
            return 0;
        }else{
            _aSeekTime = -1;
        }
    }

    // 发送压缩数据到解码器
    int ret = avcodec_send_packet(_aDecodeCtx, &pkt);
    //释放pkt
    av_packet_unref(&pkt);
//    cout << "释放pkt后pkt地址" << &pkt << endl;
    //删除头部pkt,写成引用类型，不能立刻就从list中pop_front删掉这个pkt对象的内存，后面还会用到，用完之后才能删
    _aPktList.pop_front();
//    cout << "pkt从链表中移除后" << &pkt << endl;
    _aMutex.unlock();
    RET(avcodec_send_packet);

    // 获取解码后的数据
    ret = avcodec_receive_frame(_aDecodeCtx, _aSwrInFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    } else RET(avcodec_receive_frame);
    //查看输入文件格式
    //cout << "采样率:" << _aSwrInFrame->sample_rate << "声道数:" << _aSwrInFrame->channels << "采样格式:" << av_get_sample_fmt_name((AVSampleFormat)_aSwrInFrame->format) << endl;

    //由于解码出来的pcm和SDL要求的pcm格式可能不一致，需要进行音频重采样
    //重采样输出的样本数 向上取整  48000 1024 44100  outSamples
    int outSamples = av_rescale_rnd(_aSwrOutSpec.sampleRate, _aSwrInFrame->nb_samples, _aSwrInSpec.sampleRate, AV_ROUND_UP);

    // 重采样(返回值转换后的样本数量)_aSwrOutFrame->data必须初始化，否则重采样转化的pcm样本不知道放在那儿
    ret = swr_convert(_aSwrCtx,_aSwrOutFrame->data, outSamples,(const uint8_t **) _aSwrInFrame->data, _aSwrInFrame->nb_samples);
    RET(swr_convert);
    //ret为每一个声道的样本数 * 声道数 * 每一个样本的大小 = 重采样后的pcm的大小
    return  ret * _aSwrOutSpec.bytesPerSampleFrame;
}
