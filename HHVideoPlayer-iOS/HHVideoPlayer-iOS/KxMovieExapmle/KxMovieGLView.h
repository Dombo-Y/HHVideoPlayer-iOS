//
//  KxMovieGLView.h
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/18.
//

#import <UIKit/UIKit.h>


@class KxVideoFrame;
@class KxMovieDecoder;

NS_ASSUME_NONNULL_BEGIN

@interface KxMovieGLView : UIView

- (id) initWithFrame:(CGRect)frame
             decoder: (KxMovieDecoder *) decoder;

- (void) render: (KxVideoFrame *) frame;

@end

NS_ASSUME_NONNULL_END
