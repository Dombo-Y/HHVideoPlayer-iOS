//
//  videoPlayer.cpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/5.
//

#include "videoPlayer.h"
#include <thread>

#define AUDIO_MAX_PKT_SIZE 1000
#define VIDEO_MAX_PKT_SIZE 500

#pragma mark - 构造析构
VideoPlayer::VideoPlayer()
{
    // 初始化Audio子系统
    if (SDL_Init(SDL_INIT_AUDIO)) {
        // 返回值不是0，就代表失败
        cout << "SDL_Init Error" << SDL_GetError() << endl;
//        emit playFailed(this);
        return;
    }
}
VideoPlayer::~VideoPlayer(){
    //窗口关闭停掉子线程
    stop();
    //若不该为Stopped状态，线程还在后台执行未停止
    SDL_Quit();
}
#pragma mark - 公有方法
VideoPlayer::State VideoPlayer::getState(){
     return _state;
}
void VideoPlayer:: play(){
    if(_state == VideoPlayer::Playing) return;
    //状态可能是暂停，停止，播放完毕

    if(_state == Stopped){
        std::thread([this](){
            readFile();
        }).detach();
    }else{
        //改变状态
        setState(VideoPlayer::Playing);
    }
    //解封装，解码，播放，音视频同步
    //创建子线程去解码
    //解码后的格式不一定是我们播放器想要的
    //PCM格式不是SDL支持的 S16 44100
    //YUV RGB
}
void VideoPlayer::pause(){
    if(_state != VideoPlayer::Playing) return;
    //状态可能是:正在播放，暂停，正常完毕
    //改变状态
    setState(VideoPlayer::Paused);
}

void VideoPlayer::stop(){
    if(_state == VideoPlayer::Stopped) return;
    //改变状态
    _state = Stopped;
    //释放资源
    playerfree();
    //通知外界
//    stateChanged(self);
}
bool VideoPlayer::isPlaying(){
    return _state == VideoPlayer::Playing;
}
//转成C语言字符串防止出现乱码666-> 666\0 后面要加1,写引用防止拷贝构造函数
void VideoPlayer::setFilename(const char *filename){
    printf("%s\n",filename);
    memcpy(_filename,filename,strlen(filename) + 1);
    cout << _filename << endl;
}
int VideoPlayer::getDuration(){
    //四舍五入，否则8.9会变成8，滑动条定格在距离终点9的8的位置
    //ffmpeg时间转现实时间
    return _fmtCtx ? round(_fmtCtx->duration * av_q2d(AV_TIME_BASE_Q)):0;
}
//总时长四舍五入
int VideoPlayer::getTime(){
    return round(_aTime);
}
void VideoPlayer::setTime(int seekTime){
   _seekTime = seekTime;
   cout << "setTime" << seekTime << endl;
}
void VideoPlayer::setVolumn(int volumn){
    _volumn = volumn;
}
int VideoPlayer::getVolume(){
    return _volumn;
}
void VideoPlayer::setMute(bool mute){
    _mute = mute;
}
bool VideoPlayer::isMute(){
    return _mute;
}
#pragma mark - 公有方法
void VideoPlayer::readFile(){
//        cout << "readFile线程" << endl;
        //返回结果
        int ret = 0;
        //创建解封装上下文、打开文件
        ret = avformat_open_input(&_fmtCtx,_filename,nullptr,nullptr);
        END(avformat_open_input);
        //检索流信息
        ret = avformat_find_stream_info(_fmtCtx,nullptr);
        END(avformat_find_stream_info);
        //打印流信息到控制台
        av_dump_format(_fmtCtx,0,_filename,0);
        fflush(stderr);
        //初始化音频信息
        _hasAudio = initAudioInfo() >= 0; // 耗时
        //初始化视频信息
        _hasVideo = initVideoInfo() >= 0; // 耗时
        if(!_hasAudio && !_hasVideo)
        {
            fataError();
            return;
        }

        //初始化完毕，发送信号
//        initFinished(self);
        //改变状态 要在读取线程的前面，否则导致解码循环提前退出，解码循环读取到时Stop状态直接break，再也不进入 无法解码 一直黑屏或没有声音，
        //也可能SDL音频子线程一开始在Stopped，就退出了
        setState(VideoPlayer::Playing);

        //音频解码子线程开始工作:开始播放pcm
        SDL_PauseAudio(0);

        //视频解码子线程开始工作:开启新的线程去解码视频数据
        std::thread([this](){
            decodeVideo();
        }).detach();

//        从输入文件中读取数据
        //确保每次读取到的pkt都是新的，在while循环外面，则每次加入list中的pkt都不会将一模一样，不为最后一次读取到的pkt，为全新的pkt，调用了拷贝构造函数
        AVPacket pkt;
        while(_state != Stopped){
            //处理seek操作
            if(_seekTime >= 0){
                int streamIdx;
                if(_hasAudio){//优先使用音频流索引
                cout << "seek优先使用，音频流索引" << _aStream->index << endl;
                  streamIdx = _aStream->index;
                }else{
                cout << "seek优先使用，视频流索引" << _vStream->index << endl;
                  streamIdx = _vStream->index;
                }
                //现实时间 -> 时间戳
                AVRational time_base = _fmtCtx->streams[streamIdx]->time_base;
                int64_t ts = _seekTime/av_q2d(time_base);
                ret = av_seek_frame(_fmtCtx,streamIdx,ts,AVSEEK_FLAG_BACKWARD);
                if(ret < 0){//seek失败
                    cout << "seek失败" << _seekTime << ts << streamIdx << endl;
                    _seekTime = -1;
                }else{//seek成功
                    cout << "------------seek成功-----------seekTime:" << _seekTime << "--ffmpeg时间戳:" << ts << "--流索引:" << streamIdx << endl;
                    //记录seek到了哪一帧，有可能是P帧或B,会导致seek向前找到I帧，此时就会比实际seek的值要提前几帧，现象是调到seek的帧时会快速的闪现I帧到seek的帧
                    //清空之前读取的数据包
                    clearAudioPktList();
                    clearVideoPktList();
                    _vSeekTime = _seekTime;
                    _aSeekTime = _seekTime;
                    _seekTime = -1;
                    //恢复时钟
                    _aTime = 0;
                    _vTime = 0;
                }
            }

            int vSize = static_cast<int>(_vPktList.size());//_vPktList.size();
            int aSize = static_cast<int>(_aPktList.size());//_aPktList.size()
            //不要将文件中的压缩数据一次性读取到内存中，控制下大小
            if(vSize >= VIDEO_MAX_PKT_SIZE || aSize >= AUDIO_MAX_PKT_SIZE){
                SDL_Delay(10);
                continue;
            }
//            cout << "视频包list大小:" << _vPktList.size() << "音频包list大小:" << _aPktList.size() << endl;
            ret = av_read_frame(_fmtCtx,&pkt);
            if(ret == 0){
                if(pkt.stream_index == _aStream->index){//读取到的是音频数据
//                    cout << "--------读取音频-------" << endl;
                    addAudioPkt(pkt);
                }else if(pkt.stream_index == _vStream->index){//读取到的是视频数据
//                    cout << "--------读取视频-------" << endl;
                    addVideoPkt(pkt);
                }else{//如果不是音频、视频流，直接释放，防止内存泄露
                    av_packet_unref(&pkt);
                }
            }else if(ret == AVERROR_EOF){//读到了文件尾部
                if(vSize == 0 && aSize == 0){
                    cout << "AVERROR_EOF读取到文件末尾了-------vSize=0--aSize=0--------" << endl;
                    //说明文件正常播放完毕
                    _fmtCtxCanFree = true;
                    break;
                }
                //读取到文件尾部依然要在while循环中转圈圈，若break跳出循环，则无法seek往回读了
            }else{
                ERROR_BUF;
                cout << "av_read_frame error" << errbuf;
                continue;
            }
        }
        if(_fmtCtxCanFree){//正常读取到文件尾部
            cout << "AVERROR_EOF------------读取到文件末尾了---------------" << _fmtCtxCanFree << endl;
            stop();
        }else{
            //标记一下:_fmtCtx可以释放了
            _fmtCtxCanFree = true;
        }
}
//初始化解码器:根据传入的AVMediaType获取解码信息，要想外面获取到解码上下文，传入外边的解码上下文的地址，同理传入stream的地址,里面赋值后
//外部能获取到，C语言中的指针改变传入的地址中的值
int VideoPlayer::initDecoder(AVCodecContext **decodeCtx,AVStream **stream,AVMediaType type){
    //根据TYPE寻找最合适的流信息
    //返回值是流索引
    int ret = av_find_best_stream(_fmtCtx,type,-1,-1,nullptr,0);
    RET(av_find_best_stream);
    //检验流
    int streamIdx = ret;
    //cout  << "文件的流的数量" << _fmtCtx->nb_streams << endl;
    *stream = _fmtCtx->streams[streamIdx];
    if(!*stream){
        cout << "stream is empty" << endl;
        return -1;
    }
    //为当前流找到合适的解码器
    AVCodec *decoder = nullptr;
    //音频解码器用libfdk_aac，不用ffmpeg默认自带的aac，默认自带的aac会解码成fltp格式的pcm，libfdk_aac会解码成sl6le的pcm
    if((*stream)->codecpar->codec_id == AV_CODEC_ID_AAC){
        decoder = avcodec_find_decoder_by_name("libfdk_aac");
    }else{
        decoder = avcodec_find_decoder((*stream)->codecpar->codec_id);
    }
    if(!decoder){
        cout << "decoder not found" <<(*stream)->codecpar->codec_id << endl;
        return -1;
    }
    //初始化解码上下文，打开解码器
    *decodeCtx = avcodec_alloc_context3(decoder);
    if(!decodeCtx){
        cout << "avcodec_alloc_context3 error" << endl;
        return -1;
    }
    //从流中拷贝参数到解码上下文中
    ret = avcodec_parameters_to_context(*decodeCtx,(*stream)->codecpar);
    RET(avcodec_parameters_to_context);
    //打开解码器
    ret = avcodec_open2(*decodeCtx,decoder,nullptr);
    RET(avcodec_open2);
    return 0;
}


void VideoPlayer::setState(State state){
    if(state == _state) return;
    _state = state;
    //通知播放器进行UI改变
//    stateChanged(self);
}
void VideoPlayer::playerfree(){
    while (_hasAudio && !_aCanFree);
    while (_hasVideo && !_vCanFree);
    while (!_fmtCtxCanFree);

    _seekTime = -1;
    avformat_close_input(&_fmtCtx);
    _fmtCtxCanFree = false;
    freeAudio();
    freeVideo();
}
void VideoPlayer::fataError(){
    //为了配置stop调用成功
    _state = Playing;
    stop();
    setState(Stopped);
//    playFailed(self);
    playerfree();
}
#pragma mark 调用C函数
int VideoPlayer::someMethod (void *objectiveCObject, void *aParameter)
{
    // To invoke an Objective-C method from C++, use
    // the C trampoline function
//    return playerDoSomethingWith(objectiveCObject, aParameter);
    return  1;
}

void VideoPlayer::setSelf(void *aSelf)
{
    self = aSelf;
}
