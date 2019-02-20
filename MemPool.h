#ifndef __MEMORY_POOL_H__
#define __MEMORY_POOL_H__
#include <stdint.h>
#include <pthread.h>

#define MAX_POOL_SIZE 1024 * 1024
#define MAX_POOL_CNT    8192  //limited by _used_map[256]
typedef  uint16_t CNT_TYPE;
typedef struct
{
   // struct s_map_table* pnext;
  //  void* p_start;
    CNT_TYPE prev_cnt;
    CNT_TYPE cnt;
} Memory_Map_Table;

// typedef struct s_msp_chunk
// {
//     void *memory_start; //内存池起始地址
//     void *memory_end; 
//     Memory_Map_Table *map_table;
//     uint32_t _used_map[128]; //空闲块标记
//     uint32_t block_size;
//     CNT_TYPE block_cnt;
//     struct s_msp_chunk* next;
//     CNT_TYPE _max_free_cnt;
    
//     pthread_mutex_t _m_lock;
// } Memory_Map_Chunk;
typedef struct
{
    void *memory_start; //内存池起始地址
    void *memory_end; 
    Memory_Map_Table *map_table;
    uint32_t _used_map[256]; //空闲块标记
    uint32_t block_size;
    CNT_TYPE block_cnt;
  //  uint32_t alloc_cnt;
    CNT_TYPE _max_free_cnt;
    
    pthread_mutex_t _m_lock;
} Memory_Pool;

Memory_Pool *memory_pool_init(size_t blocksize, CNT_TYPE blockcnt);
void *memory_malloc(Memory_Pool *pool, CNT_TYPE cnt);
int8_t memory_free(Memory_Pool *pool, void *memory);
void memory_pool_destroy(Memory_Pool *pool);

#endif