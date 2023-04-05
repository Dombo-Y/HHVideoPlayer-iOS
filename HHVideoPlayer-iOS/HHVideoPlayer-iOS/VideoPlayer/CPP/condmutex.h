//
//  condmutex.hpp
//  HHVideoPlayer-iOS
//
//  Created by 尹东博 on 2023/4/5.
//

#ifndef CONDMUTEX_H
#define CONDMUTEX_H
#include "SDL.h"
//extern "C" {
// #include <libSDL2/SDL.h>
//}

class CondMutex
{
public:
    CondMutex();
    ~CondMutex();

    void lock();
    void unlock();
    void signal();
    void broadcast();
    void wait();
private:
    /** 互斥锁 */
    SDL_mutex *_mutex = nullptr;
    /** 条件变量 */
    SDL_cond *_cond = nullptr;
};

#endif // CONDMUTEX_H
