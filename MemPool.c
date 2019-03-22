//这个内存池的以牺牲内存利用率为代价，换取更高的内存归还/重用效率，隔离其他进程对当前进程的影响
//1、内存利用率很低
//2、锁粒度太大
//增加测试
//在分配的内存中添加无用数据，确保用户不会对锁分配的内存内容有任何假设
//记录内存状态
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

//void
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

    pool->page_cnt = blockcnt;
    pool->page_size = blocksize % 4 == 0 ? blocksize : blocksize + 4 - blocksize % 4;

    pool->map_table = (Memory_Map_Table *)malloc(sizeof(Memory_Map_Table) * pool->page_cnt);
    pool->memory_start = malloc(pool->page_cnt * pool->page_size);

    if (pool == NULL || pool->map_table == NULL || pool->memory_start == NULL)
    {
        log_debug(mp_log, "Malloc failed\n");
        return pool;
    }

    pool->memory_end = pool->memory_start + pool->page_cnt * pool->page_size;

    memset(pool->map_table, 0, pool->page_cnt * sizeof(Memory_Map_Table));
    memset(pool->_used_map, 0, sizeof(pool->_used_map));
    // *(CNT_TYPE *)pool->memory_start = 0;
    pool->map_table[0].cnt = pool->page_cnt;
    pool->pfree_page_head = pool->memory_start;
    pool->pfree_page_head->idx = 0;
    pool->pfree_page_head->cnt = pool->page_cnt;
    pool->pfree_page_head->pNext = NULL;
    pool->pfree_page_head->pPrev = NULL;
    pool->pfree_page_tail = pool->memory_start;
    // pool->pfree_page_head->guard = 0xabcd;
    //;
    log_info(mp_log, "memory_pool_init: total size: %d, block cnt: %d, block size: %d\n",
             pool->page_cnt * pool->page_size, pool->page_cnt, pool->page_size);

    return pool;
}
inline void Delate_Node(Memory_Pool *pool, Free_Page_Info *mem)
{
    // return -2;
    // if (mem == NULL)
    // {
    //     return;
    // }
    if (mem->pNext == NULL) //是尾巴
    {
        //  printf("is tail \n");
        pool->pfree_page_tail = pool->pfree_page_tail->pPrev; //如果mem是头节点，pPrev就是空的
        if (pool->pfree_page_tail != NULL)
        {
            pool->pfree_page_tail->pNext = NULL;
        }
        else //头和尾巴要同时为NULL
        {
            pool->pfree_page_head = NULL;
        }
    }
    else if (mem->pPrev == NULL) //说明是头
    {
        // printf("is head \n");

        pool->pfree_page_head = pool->pfree_page_head->pNext; //能到这一步说明mem肯定不是尾巴,pNext肯定不是空的
                                                              // if (pool->pfree_page_head != NULL)
        pool->pfree_page_head->pPrev = NULL;
    }
    else //if ((mem->pNext != NULL) && (mem->pPrev != NULL))
    {
        //  printf("is not head or tail mem->pNext %p \n", mem->pNext);

        mem->pNext->pPrev = mem->pPrev;
        mem->pPrev->pNext = mem->pNext;
    }
}
void Add_Node(Memory_Pool *pool, Free_Page_Info *mem)
{

    if (pool->pfree_page_tail != NULL) //只要尾巴不空，头肯定不空
    {

        pool->pfree_page_tail->pNext = mem;
        mem->pPrev = pool->pfree_page_tail;
        mem->pNext = NULL;
        pool->pfree_page_tail = mem;
    }
    else //如果尾巴空了，就是整个链表空了
    {

        pool->pfree_page_head = mem;
        mem->pPrev = NULL;
        pool->pfree_page_tail = mem;
        mem->pNext = NULL;
    }
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
    // if (cnt == 0 || cnt > pool->page_cnt)
    // {
    //     //printf("Malloc cnt error ,cnt is %d\n", cnt);
    //     log_error(mp_log, "Malloc cnt error ,cnt is %d\n", cnt);
    //     return NULL;
    // }
    if (pthread_mutex_lock(&pool->_m_lock))
    {
        log_error(mp_log, "Malloc mutex lock error\n");
        return NULL;
    }
    CNT_TYPE i = 0;
    Free_Page_Info *now = pool->pfree_page_head;
    while (now != NULL) //while (i < pool->page_cnt)
    {
        i = now->idx;
        // if (now->cnt < cnt)
        // {
        //     //  printf("index %d is too samll, size is %d \n", i, pool->map_table[i].cnt);
        //     if (now->cnt == 0)
        //         exit(0);
        // }
        // if (now->guard != 0xabcd)
        // {
        //     log_error(mp_log, "mem pool over flow\n");
        //     raise(SIGSEGV); //SIGSEGV
        //     exit(1);
        // }
        if (now->cnt > cnt)
        {
            uint32_t offset = now->cnt - cnt;
            setUsed(pool, i + offset);
            if ((i + now->cnt) < pool->page_cnt)
                pool->map_table[i + now->cnt].prev_cnt = cnt;
            pool->map_table[i + offset].cnt = cnt;
            pool->map_table[i].cnt = offset;
            pool->map_table[i + offset].prev_cnt = offset;

            // *(CNT_TYPE *)(mem+cnt*pool->page_size) = i+cnt; //每个块头作为一个哨兵
            mem = ((uint8_t *)now + offset * pool->page_size);

            now->cnt = offset;
            //   printf("now->pPrev %p  %p\n", now->pPrev, now->pNext);
            // Add_Node(pool, newInfo);
            // printf("malloc set gurd %p %d %d \n",mem+cnt*pool->page_size,*(CNT_TYPE *)mem+cnt*pool->page_size,i+cnt);
            // printf("malloc i is %d prev cnt is %d\n",i,pool->map_table[i].prev_cnt);
            //  pool->map_table[i + cnt].prev_cnt = cnt;
            //     printf("new malloc index : %d address : %p size : %d \n", i, mem, cnt);
            break;
        }
        else if (now->cnt = cnt)
        {
            Delate_Node(pool, now);
            mem = now;
            setUsed(pool, i);
            break;
        }
        else
        {
            now = now->pNext;
            /* code */
        }
    }

    //  printf("before malloc address : %p  \n", mem);
    pthread_mutex_unlock(&pool->_m_lock);

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
    // if (memory < pool->memory_start || memory > pool->memory_end)
    //     return -2;

    CNT_TYPE i = (memory - pool->memory_start) / pool->page_size; //972的整数除法还是可以的
    // if (!isUsed(pool, i))                                         //空闲
    // {
    //     log_error(mp_log, "double free in pool\n");
    //     raise(SIGSEGV); //SIGSEGV
    //     exit(1);
    //     //break;
    // }
    CNT_TYPE cnt = pool->map_table[i].cnt;
    CNT_TYPE next = i + cnt;
    if (pthread_mutex_lock(&pool->_m_lock))
        return -3;
    //printf("prev free i is %d prev cnt is %d\n",i,pool->map_table[i].prev_cnt);

    CNT_TYPE prev = i - pool->map_table[i].prev_cnt;

    if ((next < pool->page_cnt) && !isUsed(pool, next)) //空闲
    {
        Free_Page_Info *pNext = (Free_Page_Info *)((uint8_t *)(pool->memory_start) + next * pool->page_size);
        cnt += pool->map_table[next].cnt;
        next += pool->map_table[next].cnt;
        //  printf("mem %p pNext %p cnt %d\n", memory,pNext,cnt);
        Delate_Node(pool, pNext);
    }

    if (!isUsed(pool, prev) && prev != i) //空闲
    {
        Free_Page_Info *pPrev = (Free_Page_Info *)((uint8_t *)(pool->memory_start) + prev * pool->page_size);
        pool->map_table[prev].cnt += cnt;
        pPrev->cnt = pool->map_table[prev].cnt;
        pool->map_table[next].prev_cnt = pool->map_table[prev].cnt;

        //  prev+=pool->map_table[prev].prev_cnt;
    }
    else
    {
        //printf("else\n");
        if (next < pool->page_cnt)
            pool->map_table[next].prev_cnt = cnt;
        pool->map_table[i].cnt = cnt;
        pool->map_table[i].prev_cnt = pool->map_table[prev].prev_cnt;
        Free_Page_Info *pNow = (Free_Page_Info *)((uint8_t *)(pool->memory_start) + i * pool->page_size);

        //   pNow->guard = 0xabcd;
        pNow->idx = i;
        pNow->cnt = cnt;
        resetUsed(pool, i); //没有将i添加到空闲链表之前，不会有任何线程修改i

        Add_Node(pool, pNow);
    }

    //printf("free i is %d prev cnt is %d\n",i,pool->map_table[i].prev_cnt);
    // pool->map_table[prev].cnt = cnt;

    //  void* mem =pool->memory_start+prev*pool->page_size;
    // *(CNT_TYPE *)(pool->memory_start + i * pool->page_size) = i; //每个块头作为一个哨兵
    //  printf("free set gurd %p %d %d \n",mem,*(CNT_TYPE *)mem,i);

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
