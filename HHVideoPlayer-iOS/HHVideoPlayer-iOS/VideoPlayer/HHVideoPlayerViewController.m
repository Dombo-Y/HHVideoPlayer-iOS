//
//  HHVideoPlayerViewController.m
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/5.
//

#import "HHVideoPlayerViewController.h"
#import "Masonry.h"

@interface HHVideoPlayerViewController ()


@property (nonatomic, strong)UIView *contentView;
@property (nonatomic, strong)UIView *topView;
@property (nonatomic, strong)UIButton *playBtn;
@property (nonatomic, strong)UIButton *backBtn;
@property (nonatomic, strong)UIButton *pauseBtn;

@property (nonatomic, strong)UIView *videoView;

@end

@implementation HHVideoPlayerViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    [self.view addSubview:self.contentView];
    [self.contentView addSubview:self.topView];
    [self.topView addSubview:self.backBtn];
    [self.topView addSubview:self.playBtn];
    [self.topView addSubview:self.pauseBtn];
    [self.contentView addSubview:self.videoView];
    [self setupSubViewLayout];
    
}

- (UIView *)contentView {
    if (!_contentView) {
        _contentView = [[UIView alloc] initWithFrame:CGRectZero];
        _contentView.backgroundColor = [UIColor redColor];
    }
    return _contentView;
}

- (UIView *)topView {
    if (!_topView) {
        _topView = [[UIView alloc] initWithFrame:CGRectZero];
        _topView.backgroundColor = [UIColor greenColor];
    }
    return _topView;
}

- (UIButton *)playBtn {
    if (!_playBtn) {
        _playBtn = [UIButton buttonWithType:UIButtonTypeCustom];
        [_playBtn setTitle:@"播放" forState:UIControlStateNormal];
        [_playBtn addTarget:self action:@selector(playMethod:) forControlEvents:UIControlEventTouchUpInside];
        _playBtn.frame = CGRectMake(0, 0, 150, 40);
        _playBtn.titleLabel.textAlignment = NSTextAlignmentCenter;
    }
    return _playBtn;
}
- (UIButton *)pauseBtn {
    if (!_pauseBtn) {
        _pauseBtn = [UIButton buttonWithType:UIButtonTypeCustom];
        [_pauseBtn setTitle:@"暂停" forState:UIControlStateNormal];
        [_pauseBtn addTarget:self action:@selector(pauseMethod:) forControlEvents:UIControlEventTouchUpInside];
        _pauseBtn.frame = CGRectMake(0, 0, 150, 40);
        _pauseBtn.titleLabel.textAlignment = NSTextAlignmentCenter;
        _pauseBtn.backgroundColor = [UIColor orangeColor];
    }
    return _pauseBtn;
}
- (UIButton *)backBtn {
    if (!_backBtn) {
        _backBtn = [UIButton buttonWithType:UIButtonTypeCustom];
        [_backBtn setTitle:@"返回" forState:UIControlStateNormal];
        [_backBtn addTarget:self action:@selector(backMethod:) forControlEvents:UIControlEventTouchUpInside];
        _backBtn.frame = CGRectMake(0, 0, 50, 40);
    }
    return _backBtn;
}

- (UIView *)videoView {
    if (!_videoView) {
        _videoView = [[UIView alloc] initWithFrame:CGRectZero];
        _videoView.backgroundColor = [UIColor yellowColor];
    }
    return _videoView;
}

- (void)setupSubViewLayout {
    [self.contentView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.left.right.bottom.equalTo(self.view);
    }];
    [self.topView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.left.equalTo(self.view.mas_safeAreaLayoutGuideLeft);
        make.right.equalTo(self.view.mas_safeAreaLayoutGuideRight);
        make.top.equalTo(self.view.mas_safeAreaLayoutGuideTop);
        make.height.mas_equalTo(@40);
    }];
    [self.backBtn mas_makeConstraints:^(MASConstraintMaker *make) {
        make.left.top.equalTo(self.topView);
        make.width.mas_equalTo(@50);
        make.height.mas_offset(@40);
    }];
    
    [self.playBtn mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(self.topView);
        make.left.equalTo(self.backBtn.mas_right);
        make.width.mas_equalTo(@150);
        make.height.mas_offset(@40);
    }];
    [self.pauseBtn mas_makeConstraints:^(MASConstraintMaker *make) {
        make.left.equalTo(self.playBtn.mas_right);
        make.top.equalTo(self.playBtn.mas_top);
        make.width.mas_equalTo(@150);
        make.height.mas_offset(@40);
    }];
    [self.videoView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.left.equalTo(self.view);
        make.top.equalTo(self.topView.mas_bottom);
        make.width.equalTo(self.view);
        make.height.mas_equalTo(@(self.view.frame.size.width * 0.75));
    }];
}


- (void)playMethod:(UIButton *) btn {
//    [[HHVideoPlayer sharedInstance] play];
//    [HHVideoPlayer sharedInstance].delegate = self;

//    NSString *filePath = [[NSBundle mainBundle] pathForResource:@"video" ofType:@"mp4"];
//    HHAVParseHandler *parseHandle = [[HHAVParseHandler alloc] initWithPath:filePath];
////    HHVideoDecoder *videoDecoder = [[HHVideoDecoder alloc] initWithFormatContext:[parseHandle getFormatContext] videoStreamIndex:[parseHandle getVideoStreamIndex]];
//    HHAudioDecoder *audioDecoder = [[HHAudioDecoder alloc] initWithFormatContext:[parseHandle getFormatContext] audioStreamIndex:[parseHandle getAudioStreamIndex]];
////    videoDecoder.delegate = self;
//    audioDecoder.delegate = self;
//    static BOOL isFindIDR = NO;
//    [parseHandle startParseGetAVPackeWithCompletionHandler:^(BOOL isVideoFrame, BOOL isFinish, AVPacket packet) {
//        if (isFinish) {
//            isFinish = NO;
////            [videoDecoder stopDecoder];
//            [audioDecoder stopDecoder];
//            dispatch_async(dispatch_get_main_queue(), ^{
//                NSLog(@"播放停止了～～～～～～～");
//            });
//            return;
//        }
//        if (isVideoFrame) {
//            if (packet.flags == 1 && isFindIDR == NO) {
//                isFindIDR = YES;
//            }
//
//            if (!isFindIDR) {
//                return;
//            }
////            [videoDecoder startDecodeVideoDataWithAVPacket:packet];
//        } else {
//            [audioDecoder startDecodeAudioDataWithAVPacket:packet];
//        }
//    }];
}

- (void)playerAudio {

}


- (void)pauseMethod:(UIButton *)btn {
    
}

- (void)backMethod:(UIButton *)btn {
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    BOOL isPortrait = UIDeviceOrientationPortrait == UIDevice.currentDevice.orientation ;
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        [self updateLayoutConstraints:coordinator isPortrait:isPortrait];
    } completion:nil];
}

- (void)updateLayoutConstraints:(id<UIViewControllerTransitionCoordinator>)coordinator
                     isPortrait:(BOOL)isPortrait {
    self.topView.hidden = !isPortrait;
    if (isPortrait) {
        [self.videoView mas_remakeConstraints:^(MASConstraintMaker *make) {
            make.left.equalTo(self.view);
            make.top.equalTo(self.topView.mas_bottom);
            make.width.equalTo(self.view);
            make.height.mas_equalTo(@(self.view.frame.size.width * 0.75));
        }];
    }
    else {
        [self.videoView mas_remakeConstraints:^(MASConstraintMaker *make) {
            make.left.equalTo(self.view.mas_safeAreaLayoutGuideLeft);
            make.top.equalTo(self.view);
            make.right.equalTo(self.view.mas_safeAreaLayoutGuideRight);
            make.height.equalTo(self.view);
        }];
    }
}


#pragma mark - C 语言函数
int playerDoSomethingWith (void *hhObjectInstance, void *parameter) {
    
    return  1;
}
#pragma mark 解码完成绘制视频帧
void playerDoDraw(void *hhObjectInstance,void *data, uint32_t w, uint32_t h) {
    
}

#pragma mark 播放器状态改变
void stateChanged(void *hhObjectInstance) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [(__bridge id)hhObjectInstance stateChanged];
    });
}

-(void)stateChanged{
    
}
#pragma mark 音视频解码器初始化完毕
-(void)initFinished{
    
}
void initFinished(void *hhObjectInstance) { 
    dispatch_async(dispatch_get_main_queue(), ^{
        [(__bridge id)hhObjectInstance initFinished];
    });
}


#pragma mark 音视频播放音频时间变化 时间Label和进度条变化
-(void)timeChanged{
    //音频播放时间设置滚动条和label
//    self.timeSlider.value = _player->getTime();
//    self.timeLabel.text = [self getTimeText:_player->getTime()];
}

#pragma mark 音视频播放音频时间变化
void timeChanged(void *hhObjectInstance) {
    
} 

#pragma mark 音视频播放失败
void playFailed(void *hhObjectInstance) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [(__bridge id)hhObjectInstance playFailed];
    });
}
-(void)playFailed{
    
}
@end
