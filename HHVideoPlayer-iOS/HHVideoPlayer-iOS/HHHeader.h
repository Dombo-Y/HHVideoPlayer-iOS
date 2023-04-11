//
//  HHHeader.h
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/12.
//

#ifndef HHHeader_h
#define HHHeader_h

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"


#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_endian.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_loadso.h"
#include "SDL_main.h"
#include "SDL_mutex.h"
#include "SDL_rwops.h"
#include "SDL_stdinc.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_video.h"

#ifdef __cplusplus
};
#endif

#endif /* HHHeader_h */
