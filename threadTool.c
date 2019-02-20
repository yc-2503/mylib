/**
* @file threadTool.c
* @brief 线程相关方法封装
* @author TOPSCOMM YC
* @date 2019-1-29
* @version V1.0.1
* @detail none

*/

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "threadTool.h"
#include <errno.h>
#include "assert.h"

void *timer_wait_handle(void *arg);

//以分离状态创建线程

int32_t makethread(void *(*fn)(void *), void *arg)
{

    int32_t err;
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
/**
 * @brief 毫秒级定时器.
 * @param timer 定时器指针，time_ms 定时时间，handle 定时器用户回调函数，arg 定时器处理函数参数.
 * @return errorno.
 * @note 内部创建了新的线程处理用户回调函数，因此精度严重依赖线程调度器，仅能用于对没有实时性要求的场合。
 */
int Make_Timer(Timer_Wait_t *timer, uint32_t time_ms, void *(*handle)(void *), void *arg)
{

    int ret = pthread_condattr_init(&(timer->cattr));

    if (ret != 0)
    {
        return (1);
    }

    ret = pthread_mutex_init(&(timer->i_mutex), NULL);
    ret = pthread_condattr_setclock(&(timer->cattr), CLOCK_MONOTONIC);
    ret = pthread_cond_init(&(timer->i_cv), &(timer->cattr));

    timer->time_ms = time_ms;
    timer->arg = arg;
    timer->handle = handle;

    ret = makethread(timer_wait_handle, (void *)timer);
    return ret;
}

void Cancel_Timer_Wait(Timer_Wait_t *timer)
{
    pthread_mutex_lock(&(timer->i_mutex));
    pthread_cond_signal(&(timer->i_cv));
    pthread_mutex_unlock(&(timer->i_mutex));

    pthread_mutex_destroy(&(timer->i_mutex));
    pthread_cond_destroy(&(timer->i_cv));
}

void *timer_wait_handle(void *arg)
{
    Timer_Wait_t *timer = (Timer_Wait_t *)arg;

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
        return NULL;
    }
    assert(timer->handle != NULL);

    timer->handle(timer->arg);

    pthread_mutex_destroy(&(timer->i_mutex));
    pthread_cond_destroy(&(timer->i_cv));
    return NULL;
}