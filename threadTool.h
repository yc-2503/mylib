#ifndef THREAD_TOOL_H_
#define THREAD_TOOL_H_
#include <stdint.h>
typedef struct
{
    pthread_condattr_t cattr;
    pthread_mutex_t i_mutex;
    pthread_cond_t i_cv;
    uint32_t time_ms;
    void *arg;
    void *(*handle)(void *);
    uint8_t is_stop;
} _timer_wait_t;
typedef _timer_wait_t* Timer_t;
//以分离状态创建线程

int32_t makethread(void *(*fn)(void *), void *arg);
int32_t Start_Timer(Timer_t timer, uint32_t time_ms, void *(*handle)(void *), void *arg);
void Stop_Timer(Timer_t timer);
int32_t Creat_Timer(Timer_t *timer);
void Timer_Destory(Timer_t *timer);
#endif