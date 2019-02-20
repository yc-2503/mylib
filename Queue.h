#ifndef _QUEUE_H_
#define _QUEUE_H_
#include <pthread.h>
#include <stdint.h>
typedef struct
{
    uint32_t _element_size;
    void *_addr;
    uint32_t _write_pos;
    uint32_t _read_pos;
    uint32_t _cache_size;
    uint32_t _element_count;
    uint32_t freeCnt;
    uint8_t _over_write_flag;
    pthread_mutex_t _q_lock;
    pthread_mutex_t _q_cond_lock;
    pthread_cond_t _q_push_cond;
    pthread_cond_t _q_pop_cond;
    void (*psHandle)();
   // int8_t (*psHdTriger)(void *);
    void (*ppHandle)();
  //  int8_t (*ppHdTriger)(void *);
  //  struct QDescriptor* pq;

} QDescriptor;
//typedef int (*phHdTriger)(QDescriptor*);
int8_t init_queue(QDescriptor *qd, void *addr, size_t elementsize, uint32_t elementcnt);
int8_t wait_push_queue(QDescriptor *qd, void *src);
int8_t push_queue(QDescriptor *qd, void *src);
uint32_t append_queue(QDescriptor *qd, void *subaddr, uint32_t cnt);
int8_t pop_queue(QDescriptor *qd, void *dst);
int8_t wait_pop_queue(QDescriptor *qd, void *dst);
uint32_t extract_queue(QDescriptor *qd, void *subaddr, uint32_t cnt);
uint32_t get_Queue_Used_Cnt(QDescriptor *qd);
void destroy_queue(QDescriptor *qd, void (*act)(void *));
#endif