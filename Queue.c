/**
 * @file Queue.c
 * @brief 队列封装
 * @author YC
 * @date 2019-01-14
 * @version V1.1.0
 * @detail 
 * 1.仅支持Linux环境，需要调用者提供缓存
 * 2.提供阻塞/非阻塞接口，线程安全
 * 3.不可重入！
 * V1.1.0更新：
 * 1、使用缓冲队列空闲长度替代读指针与写指针相对位置，作为入队与出队条件
 * 2、增加入队与出队回调函数
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "Queue.h"

//匿名管道内部是依靠文件系统实现的，对管道的操作基本上都会设计对文件的操作，因此不可避免的会陷入内核，降低效率
/**
 * @brief 初始化队列描述符
 * @param   qd 队列描述符
 *          addr 缓存首地址
 *          elementsize 元素大小
 *          elementcnt 缓存能够存储的最大元素数
 * @return 正常 0；异常 非正整数
 */
int8_t init_queue(QDescriptor *qd, void *addr, size_t elementsize, uint32_t elementcnt)
{
    qd->_addr = addr;
    qd->_element_size = elementsize;
    qd->_cache_size = elementcnt * elementsize;
    qd->_element_count = elementcnt;
    qd->_write_pos = 0;
    qd->_read_pos = 0;
    qd->_over_write_flag = 0;
    qd->freeCnt = elementcnt;
    qd->psHandle = NULL;
    qd->ppHandle = NULL;
    // if (pthread_mutex_init(&qd->_q_lock, NULL)) //初始化锁
    //     return -1;
    if (pthread_mutex_init(&qd->_q_cond_lock, NULL)) //初始化锁
        return -1;
    if (pthread_cond_init(&qd->_q_push_cond, NULL))
        return -2;
    if (pthread_cond_init(&qd->_q_pop_cond, NULL))
        return -3;
    return 0;
}
/**
 * @brief 阻塞的入队接口
 * @param   qd 队列描述符
 *          src 待入队元素的首地址
 * @return 正常 0；异常 非正整数
 */
int8_t wait_push_queue(QDescriptor *qd, void *src)
{
    if (pthread_mutex_lock(&qd->_q_cond_lock))
        return -1;

    while ((qd->_write_pos == qd->_read_pos) && qd->_over_write_flag)
    {
        if (pthread_cond_wait(&qd->_q_push_cond, &qd->_q_cond_lock))
        {
            pthread_mutex_unlock(&qd->_q_cond_lock);
            return -2;
        }
    }
    memcpy(qd->_addr + (qd->_write_pos * qd->_element_size), src, qd->_element_size);
    qd->_write_pos++;

    if (qd->_write_pos == qd->_element_count)
    {
        qd->_write_pos = 0;
        qd->_over_write_flag = 1;
    }
    qd->freeCnt--;
    pthread_cond_signal(&qd->_q_pop_cond);
    pthread_mutex_unlock(&qd->_q_cond_lock);
    if (qd->psHandle != NULL)
    {
        qd->psHandle();
    }
    return 0;
}
/**
 * @brief 非阻塞的入队接口
 * @param   qd 队列描述符
 *          tar 待入队元素的首地址
 * @return 正常 0；异常 非正整数
 */
int8_t push_queue(QDescriptor *qd, void *src)
{
    if (pthread_mutex_lock(&qd->_q_cond_lock))
        return -1;
    if ((qd->_write_pos == qd->_read_pos) && qd->_over_write_flag) //如果当前队列已满，则立即返回
    {
        pthread_mutex_unlock(&qd->_q_cond_lock);
        return -2;
    }
    memcpy(qd->_addr + (qd->_write_pos * qd->_element_size), src, qd->_element_size);
    qd->_write_pos++;

    if (qd->_write_pos == qd->_element_count)
    {
        qd->_write_pos = 0;
        qd->_over_write_flag = 1;
    }
    qd->freeCnt--;
    pthread_cond_signal(&qd->_q_pop_cond);
    pthread_mutex_unlock(&qd->_q_cond_lock);
    if (qd->psHandle != NULL)
    {
        qd->psHandle();
    }
    return 0;
}
/**
 * @brief 在队尾添加多个元素的非阻塞接口
 * @param   qd 队列描述符
 *          subaddr 待入队元素的首地址
 *          cnt 入队元素个数
 * @return 完成添加的元素个数
 */
uint32_t append_queue(QDescriptor *qd, void *subaddr, uint32_t cnt)
{
    if (pthread_mutex_lock(&qd->_q_cond_lock))
        return 0;
    if ((qd->_write_pos == qd->_read_pos) && qd->_over_write_flag)
    {
        pthread_mutex_unlock(&qd->_q_cond_lock);
        return 0;
    }
    uint32_t writecnt = 0;
    while (writecnt < cnt && qd->freeCnt > 0)
    {
        memcpy(qd->_addr + (qd->_write_pos * qd->_element_size), subaddr + (writecnt * qd->_element_size), qd->_element_size);
        qd->_write_pos++;

        if (qd->_write_pos == qd->_element_count)
        {
            qd->_write_pos = 0;
            qd->_over_write_flag = 1;
        }
        qd->freeCnt--;
        writecnt++;
    }
    pthread_cond_signal(&qd->_q_pop_cond);
    pthread_mutex_unlock(&qd->_q_cond_lock);
    if (qd->psHandle != NULL)
    {
        qd->psHandle();
    }
    return writecnt;
}
/**
 * @brief 非阻塞的出队接口
 * @param   qd 队列描述符
 *          dst 存储出队后元素内存的首地址
 * @return 正常 0；异常 非正整数
 */
int8_t pop_queue(QDescriptor *qd, void *dst)
{
    if (pthread_mutex_lock(&qd->_q_cond_lock))
        return -1;
    if (qd->freeCnt == qd->_element_count)
    {
        pthread_mutex_unlock(&qd->_q_cond_lock);
        return -2;
    }

    memcpy(dst, qd->_addr + (qd->_read_pos * qd->_element_size), qd->_element_size);
    qd->_read_pos++;
    if (qd->_read_pos == qd->_element_count)
    {
        qd->_read_pos = 0;
        qd->_over_write_flag = 0;
    }
    qd->freeCnt++;
    pthread_cond_signal(&qd->_q_push_cond);
    pthread_mutex_unlock(&qd->_q_cond_lock);
    if (qd->ppHandle != NULL)
    {
        qd->ppHandle();
    }
    return 0;
}
/**
 * @brief 阻塞的出队接口
 * @param   qd 队列描述符
 *          dst 存储出队后元素内存的首地址
 * @return 正常 0；异常 非正整数
 */
int8_t wait_pop_queue(QDescriptor *qd, void *dst)
{
    if (pthread_mutex_lock(&qd->_q_cond_lock))
        return -1;
    while (qd->freeCnt == qd->_element_count) // while (qd->_write_pos == qd->_read_pos && !qd->_over_write_flag)
    {
        if (pthread_cond_wait(&qd->_q_pop_cond, &qd->_q_cond_lock))
        {
            pthread_mutex_unlock(&qd->_q_cond_lock);
            return -2;
        }
    }

    memcpy(dst, qd->_addr + (qd->_read_pos * qd->_element_size), qd->_element_size);
    qd->_read_pos++;
    if (qd->_read_pos == qd->_element_count)
    {
        qd->_read_pos = 0;
        qd->_over_write_flag = 0;
    }
    qd->freeCnt++;
    pthread_cond_signal(&qd->_q_push_cond);
    pthread_mutex_unlock(&qd->_q_cond_lock);
    if (qd->ppHandle != NULL)
    {
        qd->ppHandle();
    }
    return 0;
}
/**
 * @brief 从队首提取多个元素的非阻塞接口
 * @param   qd 队列描述符
 *          tar 存储出队后元素内存的首地址
 *          cnt 出队元素个数
 * @return 出队元素个数
 */
uint32_t extract_queue(QDescriptor *qd, void *subaddr, uint32_t cnt)
{
    if (pthread_mutex_lock(&qd->_q_cond_lock))
        return 0;
    if (qd->freeCnt == qd->_element_count) //if (qd->_write_pos == qd->_read_pos && !qd->_over_write_flag)
    {
        pthread_mutex_unlock(&qd->_q_cond_lock);
        return 0;
    }
    uint32_t readcnt = 0;
    while (readcnt < cnt && qd->freeCnt < qd->_element_count) //   if (cnt < qd->freeCnt)
    {

        memcpy(subaddr + (readcnt * qd->_element_size), qd->_addr + (qd->_read_pos * qd->_element_size), qd->_element_size);
        qd->_read_pos++;
        if (qd->_read_pos == qd->_element_count)
        {
            qd->_read_pos = 0;
            qd->_over_write_flag = 0;
        }
        qd->freeCnt++;
        readcnt++;
    }
    pthread_cond_signal(&qd->_q_push_cond);
    pthread_mutex_unlock(&qd->_q_cond_lock);

    if (qd->ppHandle != NULL)
    {
        qd->ppHandle();
    }
    return readcnt;
}
uint32_t get_Queue_Used_Cnt(QDescriptor *qd)
{
    if (pthread_mutex_lock(&qd->_q_cond_lock))
        return 0;
    uint32_t cnt= qd->_element_count-qd->freeCnt;
    pthread_mutex_unlock(&qd->_q_cond_lock);
    return cnt;
}
void destroy_queue(QDescriptor *qd, void (*act)(void *))
{
    if (act != NULL) //用户垃圾处理程序
    {
        act(qd->_addr);
    }
}
