/**
* @file threadTool.c
* @brief 线程相关方法封装
* @author TOPSCOMM YC
* @date 2019-1-29
* @version V1.0.2
* @detail none

*/

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "threadTool.h"
#include <errno.h>
#include "assert.h"

void *timer_wait_handle(void *arg);
pthread_mutex_t destory_mutex = PTHREAD_MUTEX_INITIALIZER; //销毁定时器用到的锁
//以分离状态创建线程

int32_t makethread(void *(*fn)(void *), void *arg)
{

    int32_t err=0;
    pthread_t tid;
    pthread_attr_t attr;

    err = pthread_attr_init(&attr);

    if (err != 0)
        return err;

    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (err == 0)
        err = pthread_create(&tid, &attr, fn, arg);

    pthread_attr_destroy(&attr);

    return err;
}
//创建一个基于条件变量的定时器
int32_t Creat_Timer(Timer_t *timer)
{
    _timer_wait_t *_timer = (_timer_wait_t *)malloc(sizeof(_timer_wait_t));
    if (_timer == NULL)
    {
        return -1;
    }
    *timer = _timer;
    pthread_condattr_init(&(_timer->cattr));

    pthread_mutex_init(&(_timer->i_mutex), NULL);
    pthread_condattr_setclock(&(_timer->cattr), CLOCK_MONOTONIC);
    pthread_cond_init(&(_timer->i_cv), &(_timer->cattr));
    return 0;
}

/**
 * @brief 毫秒级定时器.
 * @param timer 定时器指针，time_ms 定时时间，handle 定时器用户回调函数，arg 定时器处理函数参数.
 * @return errorno.
 * @note 内部创建了新的线程处理用户回调函数，因此精度严重依赖线程调度器，仅能用于对没有实时性要求的场合。
 */

int32_t Start_Timer(Timer_t _timer, uint32_t time_ms, void *(*handle)(void *), void *arg)
{
    if (_timer == NULL)
        return -1;
    //_timer_wait_t *_timer = timer;
    _timer->time_ms = time_ms;
    _timer->arg = arg;
    _timer->handle = handle;
    _timer->is_stop = 0;
    int32_t ret = makethread(timer_wait_handle, (void *)_timer);
    return ret;
}
//用于提前终止定时器
void Stop_Timer(Timer_t _timer)
{
    pthread_mutex_lock(&destory_mutex);
    if (_timer == NULL)
    {
        pthread_mutex_unlock(&destory_mutex);
        return;
        
    }
  //  _timer_wait_t *_timer = timer;          //这时如果timer被销毁，这里就会陷入死锁，所以 还是再加一个锁吧，但是再加一个锁就会有死锁的问题，需要好好确认时序
   // printf("timer point is %p icv point is %p\n",_timer,&(((_timer_wait_t*)_timer)->i_cv));
    pthread_mutex_lock(&(_timer->i_mutex)); //这里加锁的目的是为了 确保条件变量已经开始等待
    pthread_mutex_unlock(&destory_mutex);
    _timer->is_stop = 1;                    //防止定时器线程被调度之前就已经取消

    pthread_cond_signal(&(_timer->i_cv));
    pthread_mutex_unlock(&(_timer->i_mutex));
}

void *timer_wait_handle(void *arg)
{
    _timer_wait_t *timer = (_timer_wait_t *)arg;
    pthread_mutex_lock(&destory_mutex);
    if (timer == NULL)
    {
        pthread_mutex_unlock(&destory_mutex);
        return NULL;
    }
     
    pthread_mutex_lock(&(timer->i_mutex)); 
    pthread_mutex_unlock(&destory_mutex);

    if (timer->is_stop) //如果线程被调度之前就已经取消定时
    {
        pthread_mutex_unlock(&(timer->i_mutex));
        return NULL;
    }

    
    long nsec = (timer->time_ms % 1000) * 1000000;
    uint32_t s = timer->time_ms / 1000;

    struct timespec outtime;
    clock_gettime(CLOCK_MONOTONIC, &outtime);

    if (1000000000 - outtime.tv_nsec <= nsec)
    {
        outtime.tv_sec += s + 1;
        outtime.tv_nsec = nsec + outtime.tv_nsec - 1000000000;
    }
    else
    {
        outtime.tv_nsec += nsec;
        outtime.tv_sec += s;
    }

    int ret = pthread_cond_timedwait(&(timer->i_cv), &(timer->i_mutex), &outtime);

    if (ret != ETIMEDOUT) //超时之前，取消
    {

        pthread_mutex_unlock(&(timer->i_mutex));
        return NULL;
    }
    // assert(timer->handle != NULL);
    if (timer->handle == NULL)
    {
        printf("timer handle is NULL\n");
        pthread_mutex_unlock(&(timer->i_mutex));
        return NULL;
    }
    void *(*handle)(void *) = timer->handle;
    timer->is_stop = 0;
    pthread_mutex_unlock(&(timer->i_mutex));
    handle(timer->arg);


    return NULL;
}
//定时器销毁，主要是销毁定时器的各种锁，定时器一旦销毁，不可再使用
void Timer_Destory(Timer_t *timer)
{
    pthread_mutex_lock(&destory_mutex);
    
    if (timer == NULL)
    {
        pthread_mutex_unlock(&destory_mutex);
        return;
    }
    
    _timer_wait_t *_timer = *timer;
    pthread_mutex_lock(&(_timer->i_mutex));


    *timer=NULL;
    pthread_mutex_unlock(&destory_mutex);

    
    _timer->is_stop = 1;
    pthread_cond_signal(&(_timer->i_cv));
      

    pthread_mutex_unlock(&(_timer->i_mutex));

    pthread_condattr_destroy(&(_timer->cattr));
    pthread_mutex_destroy(&(_timer->i_mutex));
    pthread_cond_destroy(&(_timer->i_cv));
    free(_timer);
}