//
//  videoPlayer.cpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/5.
//

#include "videoPlayer.h"
#include <thread>
#include "cmdutils.h"

#define AUDIO_MAX_PKT_SIZE 1000
#define VIDEO_MAX_PKT_SIZE 500

AVDictionary *format_opts, *codec_opts, *resample_opts;
static const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = {0};

static AVPacket flush_pkt;

#pragma mark - 构造析构
VideoPlayer::VideoPlayer()
{
    // 初始化Audio子系统
    if (SDL_Init(SDL_INIT_AUDIO)) { // 返回值不是0，就代表失败
        cout << "SDL_Init Error" << SDL_GetError() << endl;
        return;
    }
}

VideoPlayer::~VideoPlayer(){ //窗口关闭停掉子线程
    stop(); //若不该为Stopped状态，线程还在后台执行未停止
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
    }else{ //改变状态
        setState(VideoPlayer::Playing);
    }
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
    stateChanged(self);
}
bool VideoPlayer::isPlaying(){
    return _state == VideoPlayer::Playing;
}

#pragma mark - thread
static int audio_thread(void *arg)
{
    return 0;
}

#pragma mark - setFilename
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
#pragma mark - FrameQueue
static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList *pkt1;

    if (q->abort_request)
       return -1; // 首先检查队列是否已经被中止（如果队列中止，那么就不能再往其中添加AVPacket，所以函数返回-1）。

    pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList)); // 然后申请一个MyAVPacketList结构体的空间，并把AVPacket数据存储在其中
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    if (pkt == &flush_pkt) // 如果这个AVPacket是一个flush_pkt（即用来清空队列的特殊包），则序列号+1
        q->serial++;
    pkt1->serial = q->serial;

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++; // 然后将这个结构体加入到队列中，更新队列的状态（nb_packets, size, duration）并通过SDL_CondSignal函数唤醒在队列上等待的线程
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->duration += pkt1->pkt.duration;
    /* XXX: should duplicate packet data in DV case */
    SDL_CondSignal(q->cond);
    return 0; // 函数返回0表示添加AVPacket到队列中成功。
//    这个函数是FFmpeg中实现音频/视频数据解码的一个基础组件，用于将从媒体文件中读取的AVPacket存储到队列中，并准备被解码。
}

// 该函数可能是FFmpeg中视频解码器的初始化函数之一，用于初始化解码器相关的数据结构。
static void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) {
    memset(d, 0, sizeof(Decoder));
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
    d->pkt_serial = -1;
}

static void packet_queue_start(PacketQueue *q)
{
    SDL_LockMutex(q->mutex); // 锁定互斥锁，保证线程安全。
    q->abort_request = 0; // 清除队列中止标志位，表示队列可以开始正常运行。
    packet_queue_put_private(q, &flush_pkt); // 在队列中加入一个名为flush_pkt的数据包，作为队列的起始标志，可以清空队列中的所有数据
    SDL_UnlockMutex(q->mutex); // 解锁互斥锁，释放线程安全控制。
}

static int decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void* arg)
{
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid) {
        av_log(NULL, AV_LOG_ERROR, "SDL_CreateThread(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}

static int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last) {
    int i ;
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = SDL_CreateMutex())) { // 初始化Mutex
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = SDL_CreateCond())) {  // 初始化cond
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    f->pktq = pktq; //
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    for (i = 0; i < f->max_size; i++)
        if (!(f->queue[i].frame = av_frame_alloc())) // 该函数还分配并初始化帧队列中每个帧的内存
            return AVERROR(ENOMEM); // 它使用 av_frame_alloc 函数为每个帧分配 AVFrame 实例，并将其存储在 Frame 结构体中
    return 0;
}

static int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    
    return 0;
}

static int stream_component_open(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic; // 获取VideoState结构体中的AVFormatContext指针，该指针包含了媒体文件的相关信息。
    AVCodecContext *avctx;
    AVCodec *codec;
    const char *forced_codec_name = NULL;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = 0;
    
    if (stream_index < 0 || stream_index >= ic->nb_streams)
         return -1;
    
    avctx = avcodec_alloc_context3(NULL);
    
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
   
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);
    
    avctx->codec_id = codec->id; // 它首先将找到的解码器的ID分配给AVCodecContext结构体的codec_id字段
    avctx->lowres = stream_lowres;
    ret = avcodec_open2(avctx, codec, nullptr);
    
    is->eof = 0; // 首先将eof标志位设为0，然后将流的丢弃标志设置为AVDISCARD_DEFAULT
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:{
            sample_rate    = avctx->sample_rate;
            nb_channels    = avctx->channels;
            channel_layout = avctx->channel_layout;
            ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt);
            is->audio_hw_buf_size = ret;
            is->audio_src = is->audio_tgt;
            is->audio_buf_size  = 0;
            is->audio_buf_index = 0;
            is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
            is->audio_diff_avg_count = 0;
            
            is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;
            is->audio_stream = stream_index;
            is->audio_st = ic->streams[stream_index];
            
            decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
             if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
                 is->auddec.start_pts = is->audio_st->start_time;
                 is->auddec.start_pts_tb = is->audio_st->time_base;
             }
            ret = decoder_start(&is->auddec, audio_thread, "audio_decoder", is);
            SDL_PauseAudioDevice(0, 0);
        }
            break;
            
        default:
            break;
    }
    
    
    return 0;
}

#pragma mark - PacketQueue
static int packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue)); // 清空PacketQueue数据结构，并将其所有元素初始化为0.
    q->mutex = SDL_CreateMutex(); // 使用SDL_CreateMutex()创建一个互斥量，以用于保护PacketQueue数据结构的访问。
    if (!q->mutex) {// 使用SDL_CreateCond()创建条件变量，以用于等待并通知PacketQueue数据结构上的数据已被更新。
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM); // 如果创建互斥量或条件变量失败，则输出错误信息并返回错误码AVERROR(ENOMEM)。
    }
    q->abort_request = 1; // 将中断请求标志位abort_request设置为1。
    return 0;
}

#pragma mark - is_realtime
static int is_realtime(AVFormatContext *s)
{
    if(   !strcmp(s->iformat->name, "rtp")
       || !strcmp(s->iformat->name, "rtsp")
       || !strcmp(s->iformat->name, "sdp")
    )
        return 1;

    if(s->pb && (   !strncmp(s->url, "rtp:", 4)
                 || !strncmp(s->url, "udp:", 4)
                )
    )
        return 1;
    return 0;
}

static int frame_queue_nb_remaining(FrameQueue *f)
{ // 段代码用于获取帧队列（FrameQueue）中尚未读取的帧数
    return f->size - f->rindex_shown;
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;
    SDL_LockMutex(q->mutex);// 该函数会使用一个互斥锁来保证线程安全，然后调用 packet_queue_put_private 函数将数据包添加到队列中
    ret = packet_queue_put_private(q, pkt);
    SDL_UnlockMutex(q->mutex);
    if (pkt != &flush_pkt && ret < 0) // 如果该函数返回负数，表示数据包添加失败，此时会将数据包的引用计数减一以释放资源。
        av_packet_unref(pkt); // 判断 pkt != &flush_pkt 的条件是为了避免释放 flush_pkt 这个空数据包（其实是一种特殊的数据包，用于表示“清空队列”）
    return ret;
}

static int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket pkt1, *pkt = &pkt1; // 定义一个静态函数packet_queue_put_nullpacket，它接受两个参数：一个PacketQueue类型的指针q和一个整数类型的stream_index，表示要放入的空包的流索引
    av_init_packet(pkt);// 在函数内部，定义了一个AVPacket类型的结构体pkt1，通过指针pkt指向pkt1。然后使用av_init_packet函数初始化pkt，将pkt的所有成员变量清零，即将data和size设置为0
    pkt->data = NULL;// 将stream_index赋值给pkt的stream_index成员变量，表示这个空包属于哪个流。
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);// 最后，调用packet_queue_put函数将pkt放入音视频队列q中，函数返回放入队列是否成功的标志位（成功返回1，失败返回0）。
}

#pragma mark - clock
static void set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

static void set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

static void init_clock(Clock *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

#pragma mark - 公有方法

static int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    bool cStreamId = stream_id < 0 ;
    bool cAbortRequest = queue->abort_request > 0;
    return cStreamId || cAbortRequest || (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
    ((queue->nb_packets > MIN_FRAMES) && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0));
}

static int read_thread(void *arg)
{
    VideoState *is = (VideoState *)arg;
    AVFormatContext *ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    AVDictionaryEntry *t;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int64_t pkt_ts;
     
    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
    }
    memset(st_index, -1, sizeof(st_index));
    is->eof = 0;
    
    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
     }
    err = avformat_open_input(&ic, is->filename, nullptr, nullptr);
    if (err < 0) {
        cout << "error -----" << is->filename<<endl ;
        ret = -1;
     }
    is->ic = ic;
    av_format_inject_global_side_data(ic);
    
    is->realtime = is_realtime(ic); 
    
    for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL; // AVDISCARD_ALL 表示全部丢弃
    }
    
    st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    st_index[AVMEDIA_TYPE_AUDIO] =  av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,  st_index[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO], NULL, 0);
//    is->show_mode = show_mode;
    
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters *codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
        if (codecpar->width) {
            // 设置获取到的  width 和 height
        }
    }
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]); // 打开音频
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
//        ret = stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]); // 打开视频
    }
    
    if (infinite_buffer < 0 && is->realtime)
        infinite_buffer = 1;
    
    for (;;) {
//        if (is->abort_request)
//            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused) {
                is->read_pause_return = av_read_pause(ic);
            }else {
                av_read_play(ic);
            }
        }
            if (is->seek_req) { // seek
                
            }
//            if(is->queue_attachments_req) { //将输入流中的附加图片放到 视频流队列中
//
//            }
            
            bool conditionA = infinite_buffer<1;
            bool conditionB = is->audioq.size + is->videoq.size > MAX_QUEUE_SIZE;//音视频累积
            bool conditionC = stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq);
            bool conditionD = stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq);
            if (conditionA && (conditionB || (conditionC && conditionD))) {
                SDL_LockMutex(wait_mutex);
                SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
                SDL_UnlockMutex(wait_mutex);
                continue;
            }
            ret = av_read_frame(ic, pkt);
            if (ret < 0) {
                if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                    if (is->video_stream >= 0){
                        packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                    }
                    if (is->audio_stream >= 0){
                        packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                    }
                    is->eof = 1;
                }
                if (ic->pb && ic->pb->error)
                    break;
                SDL_LockMutex(wait_mutex);
                SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
                SDL_UnlockMutex(wait_mutex);
                continue;
            } else {
                is->eof = 0;
            }
           
           bool temp = (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) * av_q2d(ic->streams[pkt->stream_index]->time_base) - (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000 <= ((double)duration / 1000000);
            
            stream_start_time = ic->streams[pkt->stream_index]->start_time;
            pkt_ts = pkt->pts = AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
            pkt_in_play_range = duration == AV_NOPTS_VALUE || temp;
            if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
                packet_queue_put(&is->audioq, pkt);
            }else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                      && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                packet_queue_put(&is->videoq, pkt);
            } else {
                av_packet_unref(pkt);
            }
        }
        ret = 0;
    return 0;
}

void VideoPlayer::initVideoState() {
    VideoState *iv;
    iv = (VideoState *)av_malloc(sizeof(VideoState)); // 初始化 VideoState
    if (!iv) {
//        return NULL;
    }
    
    iv->last_video_stream = iv->video_stream = -1;// 初始化 视频 stream头尾标记
    iv->last_audio_stream = iv->audio_stream = -1;// 初始化 音频 stream头尾标记
    iv->filename = _filename;
    
//    初始化解码后的帧队列
    if (frame_queue_init(&iv->pictq, &iv->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
//        return NULL;
    if (frame_queue_init(&iv->sampq, &iv->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
//        return NULL;
    
//    初始化解码前的帧队列
    if (packet_queue_init(&iv->videoq) <0 ||
        packet_queue_init(&iv->audioq) < 0) {
//        return NULL;
    }
    if(!(iv->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
//        return NULL;
    }
    
    init_clock(&iv->vidclk, &iv->videoq.serial);
    init_clock(&iv->audclk, &iv->audioq.serial);
    init_clock(&iv->extclk, &iv->extclk.serial);
    iv->audio_clock_serial = -1;
    
    iv->av_sync_type = av_sync_type;// 音频时钟同步
    iv->read_tid = SDL_CreateThread(read_thread, "read_thread", iv);
   
    if (iv->read_tid) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
    }
    is = iv; 
}

void VideoPlayer::readFile() {
   
    initVideoState();
     
    int ret = 0;
    ret = avformat_open_input(&_fmtCtx,_filename,nullptr,nullptr); //创建解封装上下文、打开文件
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
    initFinished(self);
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
                addAudioPkt(pkt); // 读取音频
            }else if(pkt.stream_index == _vStream->index){//读取到的是视频数据
                addVideoPkt(pkt); // 读取视频
            }else{
                av_packet_unref(&pkt); //如果不是音频、视频流，直接释放，防止内存泄露
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
   
    int ret = av_find_best_stream(_fmtCtx,type,-1,-1,nullptr,0);  //返回值是流索引
    RET(av_find_best_stream);
   
    int streamIdx = ret; //检验流
    //cout  << "文件的流的数量" << _fmtCtx->nb_streams << endl;
    *stream = _fmtCtx->streams[streamIdx];
    if(!*stream){
        cout << "stream is empty" << endl;
        return -1;
    }
   
    AVCodec *decoder = nullptr;  //为当前流找到合适的解码器
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
    stateChanged(self);    //通知播放器进行UI改变
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
    playFailed(self);
    playerfree();
}

#pragma mark 调用C函数
int VideoPlayer::someMethod (void *objectiveCObject, void *aParameter)
{ 
    return playerDoSomethingWith(objectiveCObject, aParameter); 
}

void VideoPlayer::setSelf(void *aSelf)
{
    self = aSelf;
}
