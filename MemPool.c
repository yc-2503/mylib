//这个内存池的以牺牲内存利用率为代价，换取更高的内存归还/重用效率，隔离其他进程对当前进程的影响
//1、内存利用率很低
//2、锁粒度太大
#include "log.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "MemPool.h"

#define MAX_FILE_SIZE 1024000 //byte
#define MAX_BACK_FILE_NUM 10
#define IO_BUFFER_SIZE 10240

log_t *mp_log;
#define setUsed(pool, i) ((pool)->_used_map[(i) >> 5] |= (0x1 << ((i)&31)))
#define resetUsed(pool, i) ((pool)->_used_map[(i) >> 5] &= ~(0x1 << ((i)&31)))
#define isUsed(pool, i) ((pool)->_used_map[(i) >> 5] & (0x1 << ((i)&31)))

/**
 * @note 初始化内存池
 * @brief 按照给定的块的大小与块的数量初始化内存池，默认会对块的大小进行4字节补齐，因此实际申请的内存的大小会大于所需要的的大小。
 * @param blocksize:内存池分配内存内存大小的基本单位
 *        blockcnt:内存池最大分配的块的数量。
 */
Memory_Pool *memory_pool_init(size_t blocksize, CNT_TYPE blockcnt)
{
    char name1[64] = {0};
    sprintf(name1, "./log/mp_log.txt");
    mp_log = log_create(name1, MAX_FILE_SIZE, MAX_BACK_FILE_NUM, IO_BUFFER_SIZE);
    Memory_Pool *pool = NULL;
    if (blockcnt > MAX_POOL_CNT)
    {
        log_debug(mp_log, "memory_pool_init(): Invalid size(%d)\n", blocksize * blockcnt);
        return pool;
    }
    if (blocksize * blockcnt < 0 || blocksize * blockcnt > MAX_POOL_SIZE)
    {
        log_debug(mp_log, "memory_pool_init(): Invalid size(%d)\n", blocksize * blockcnt);
        return pool;
    }
    pool = (Memory_Pool *)malloc(sizeof(Memory_Pool));
    if (pthread_mutex_init(&pool->_m_lock, NULL)) //初始化锁
        return NULL;

    pool->block_cnt = blockcnt;
    pool->block_size = blocksize % 4 == 0 ? blocksize : blocksize + 4 - blocksize % 4;

    pool->map_table = (Memory_Map_Table *)malloc(sizeof(Memory_Map_Table) * pool->block_cnt);
    pool->memory_start = malloc(pool->block_cnt * pool->block_size);

    if (pool == NULL || pool->map_table == NULL || pool->memory_start == NULL)
    {
        log_debug(mp_log, "Malloc failed\n");
        return pool;
    }

    pool->memory_end = pool->memory_start + pool->block_cnt * pool->block_size;

    memset(pool->map_table, 0, pool->block_cnt * sizeof(Memory_Map_Table));
    memset(pool->_used_map, 0, sizeof(pool->_used_map));
    *(CNT_TYPE *)pool->memory_start = 0;
    pool->map_table[0].cnt = pool->block_cnt;
    log_info(mp_log, "memory_pool_init: total size: %d, block cnt: %d, block size: %d\n",
             pool->block_cnt * pool->block_size, pool->block_cnt, pool->block_size);
    printf("memory_pool_init: total size: %d, block cnt: %d, block size: %d block start %p\n",
          pool->block_cnt * pool->block_size, pool->block_cnt, pool->block_size,pool->memory_start);

    return pool;
}
/**
 * @note 从内存池获取内存
 * @brief 按照给定数量，从内存池分配内存。
 * @param pool：从此内存池分配内存
 *        cnt: 需要分配的块的数量。
 */
void *memory_malloc(Memory_Pool *pool, CNT_TYPE cnt)
{
    void *mem = NULL;
    if (cnt == 0 || cnt > pool->block_cnt)
    {
        //printf("Malloc cnt error ,cnt is %d\n", cnt);
        log_error(mp_log, "Malloc cnt error ,cnt is %d\n", cnt);
        return NULL;
    }
    if (pthread_mutex_lock(&pool->_m_lock))
    {
        log_error(mp_log, "Malloc mutex lock error\n");
        return NULL;
    }
    CNT_TYPE i = 0;
    while (i < pool->block_cnt)
    {
        if (isUsed(pool, i)) //(pool->map_table[i].used)
        {
             // printf("index %d is used, size is %d \n",i,pool->map_table[i].cnt);
            i += pool->map_table[i].cnt;
            // continue;
        }
        else if (pool->map_table[i].cnt < cnt)
        {
               printf("index %d is too samll, size is %d \n",i,pool->map_table[i].cnt);
               if(pool->map_table[i].cnt==0)
               exit(0);
            i += pool->map_table[i].cnt;
            // continue;
        }
        else
        {
            mem = pool->memory_start + i * pool->block_size; //pool->memory_start+i*pool->block_size; //
            if (*(CNT_TYPE *)mem != i)
            {
               printf("memory pool slop over %p %d %d \n",mem,*(CNT_TYPE *)mem,i);
                log_error(mp_log, "memory pool slop over\n");
               raise(SIGSEGV); //SIGSEGV
            }
            if (pool->map_table[i].cnt > cnt)
            {
                resetUsed(pool, i);
                pool->map_table[i + pool->map_table[i].cnt].prev_cnt = pool->map_table[i].cnt - cnt;
                pool->map_table[i + cnt].cnt = pool->map_table[i].cnt - cnt;
                pool->map_table[i + cnt].prev_cnt = cnt;

                *(CNT_TYPE *)(mem+cnt*pool->block_size) = i+cnt; //每个块头作为一个哨兵
               // printf("malloc set gurd %p %d %d \n",mem+cnt*pool->block_size,*(CNT_TYPE *)mem+cnt*pool->block_size,i+cnt);
            }

            setUsed(pool, i); //pool->map_table[i].used = 1;
                              //   printf("%X\n",pool->_used_map[(i) >>5]);
            pool->map_table[i].cnt = cnt;
           // printf("malloc i is %d prev cnt is %d\n",i,pool->map_table[i].prev_cnt);
          //  pool->map_table[i + cnt].prev_cnt = cnt;
      //     printf("new malloc index : %d address : %p size : %d \n", i, mem, cnt);
            break;
        }
    }
    //  printf("before malloc address : %p  \n", mem);
    pthread_mutex_unlock(&pool->_m_lock);
    //  printf("after malloc address : %p  \n", mem);

    return mem;
}
/**
 * @note 向内存池释放内存
 * @brief 。
 * @param pool：向此内存池释放内存
 *        memory: 需要释放的内存的首地址。
 * @return 释放正常返回 0 失败返回负值
 */
int8_t memory_free(Memory_Pool *pool, void *memory)
{
    if (memory < pool->memory_start || memory > pool->memory_end)
        return -2;
    if (pthread_mutex_lock(&pool->_m_lock))
        return -3;

    CNT_TYPE i = (memory - pool->memory_start) / pool->block_size;
    if (!isUsed(pool, i)) //空闲
    {
        log_error(mp_log, "double free in pool\n");
        raise(SIGSEGV); //SIGSEGV
        exit(1);
        //break;
    }
    else
    {
        resetUsed(pool, i);
//printf("prev free i is %d prev cnt is %d\n",i,pool->map_table[i].prev_cnt);

        CNT_TYPE prev = i - pool->map_table[i].prev_cnt;
        CNT_TYPE next = i + pool->map_table[i].cnt;
        CNT_TYPE cnt = pool->map_table[i].cnt;
        if (!isUsed(pool, next)) //空闲
        {
            cnt += pool->map_table[next].cnt;
            next += pool->map_table[next].cnt;
        }

        if (!isUsed(pool, prev)&&prev!=i) //空闲
        {
            cnt += pool->map_table[prev].cnt;
            i=prev;
          //  prev+=pool->map_table[prev].prev_cnt;

        }


        pool->map_table[next].prev_cnt = cnt;
        pool->map_table[i].cnt=cnt;
        pool->map_table[i].prev_cnt=pool->map_table[prev].prev_cnt;
//printf("free i is %d prev cnt is %d\n",i,pool->map_table[i].prev_cnt);
       // pool->map_table[prev].cnt = cnt;
       
      //  void* mem =pool->memory_start+prev*pool->block_size;
        *(CNT_TYPE *)(pool->memory_start+i*pool->block_size) = i; //每个块头作为一个哨兵
      //  printf("free set gurd %p %d %d \n",mem,*(CNT_TYPE *)mem,i);
    }

    pthread_mutex_unlock(&pool->_m_lock);
    return 0;
}
//销毁此内存池
void memory_pool_destroy(Memory_Pool *pool)
{
    pthread_mutex_destroy(&pool->_m_lock);

    if (pool == NULL)
    {
        printf("memory_pool_destroy: pool is NULL\n");
        return;
    }
    free(pool->map_table);
    free(pool->memory_start);
    free(pool);
    pool = NULL;
}
