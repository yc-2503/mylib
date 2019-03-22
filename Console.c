#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <pthread.h>
#include "Queue.h"
#include "MemPool.h"
#include "Console.h"
#include "threadTool.h"
#include <stdarg.h>
typedef struct
{
    /* data */
    uint32_t len;
    char *str;
} Console;
int8_t _Console_EN = 1;
QDescriptor cqd;
Console cCache[128]; //暂定缓冲队列最大缓冲128个字符串。
Memory_Pool *mp;
// void DisConsole()
// {
//     _Console_EN = 0;
// }
// void EnConsole()
// {
//     _Console_EN = 1;
// }
void InitConsole()
{
    mp = memory_pool_init(32, 1024); //这个内存池并不适合分配小内存块
    init_queue(&cqd, cCache, sizeof(Console), 128);
    Console_Out();
}

void _WriteLine(const char *format, ...)
{
    // if (!_Console_EN)
    //     return;
    char buff[4096]; //字符串最大长度4096字节

    va_list ap;
    va_start(ap, format); //将ap指向第一个实际参数的地址

    vsprintf(buff, format, ap);
    va_end(ap);

    uint32_t len = strlen(buff) + 1;
    buff[len - 1] = '\n'; //换行符
    char *p = memory_malloc(mp, (len >> 5) + 1);
    if (p == NULL)
    {
        printf("mamory pool malloc memory fail at %s %d \n", __FILE__, __LINE__);
    }
    memcpy(p, buff, len + 1);
    Console console;
    console.str = p;
    console.len = len;
    wait_push_queue(&cqd, &console); //入队操作会逐字节的将数据复制到缓冲区。
}
void _Write(const char *format, ...)
{
    // if (!_Console_EN)
    //     return;
    char buff[4096]; //字符串最大长度4096字节

    va_list ap;
    va_start(ap, format); //将ap指向第一个实际参数的地址

    vsprintf(buff, format, ap);
    va_end(ap);

    uint32_t len = strlen(buff);
    //  buff[len - 1] = 10; //换行符
    char *p = memory_malloc(mp, (len >> 5) + 1);
    if (p == NULL)
    {
        printf("mamory pool malloc memory fail at %s %d \n", __FILE__, __LINE__);
    }
    memcpy(p, buff, len + 1);
    Console console;
    console.str = p;
    console.len = len;
    wait_push_queue(&cqd, &console); //入队操作会逐字节的将数据复制到缓冲区。
}

void *_outLine(void *arg)
{
    // if (!_Console_EN)
    //     return;
    char buff[4096];

    while (1)
    {
        uint32_t cnt = get_Queue_Used_Cnt(&cqd);
        uint32_t i = 0;
        Console console[cnt];
        extract_queue(&cqd, &console, cnt);
        while (cnt > 0)
        {
            uint16_t len = 0;
            while (cnt > 0)
            {
                len += console[i].len;
                if (len > 4096)
                {
                    len -= console[i].len;
                    break;
                }

                memcpy(buff + len - console[i].len, console[i].str, console[i].len);
                int8_t ret = memory_free(mp, console[i].str);
                if (ret)
                {
                    printf("mamory pool free memory fail at %s %d  ret is %d \n", __FILE__, __LINE__, ret);
                }
                i++;
                cnt--;
            }
            //  printf("write len is %d\n", len);
            write(STDOUT_FILENO, buff, len);
        }
        usleep(3000);
    }

    return NULL;
    //
}
void _console_out()
{
    makethread(_outLine, NULL);
}