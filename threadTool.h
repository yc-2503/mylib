#ifndef THREAD_TOOL_H_
#define THREAD_TOOL_H_
#include <stdint.h>
typedef struct 
{
    pthread_condattr_t cattr;
    pthread_mutex_t i_mutex;
    pthread_cond_t i_cv;
    uint32_t time_ms;
    void* arg;
    void *(*handle)(void *);
} Timer_Wait_t;

//以分离状态创建线程

int32_t makethread(void *(*fn)(void *),void *arg);
int Make_Timer(Timer_Wait_t *timer,uint32_t time_ms, void *(*handle)(void *),void *arg);
void Cancel_Timer_Wait(Timer_Wait_t *timer);

#endif