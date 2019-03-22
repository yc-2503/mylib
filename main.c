#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "Queue.h"
#include "MemPool.h"
#include "Console.h"
#include "threadTool.h"
#include <sys/time.h>
#include "md5.h"
#include <sys/ioctl.h>
#include <fcntl.h>
typedef struct
{
    pthread_t tid;
    int cnt;
} Test;
Test testQueen[10];
QDescriptor qd;
//非阻塞入队
void *tast_push_queue(void *arg)
{
    Test t;
    t.tid = pthread_self();
    while (1)
    {
        t.cnt++;
        int8_t ret = push_queue(&qd, &t);
        if (ret != 0)
        {
            //  printf("push_queue return %d \n",ret);
        }
        usleep(2000);
    }
}
//阻塞入队
void *tast_wait_push_queen(void *arg)
{
    Test t;
    t.tid = pthread_self();
    while (1)
    {
        t.cnt++;
        wait_push_queue(&qd, &t);
        usleep(9000);
    }
}
//队尾添加多个元素
void *tast_append_queen(void *arg)
{
    Test t[4];
    uint32_t cnt = 0;
    while (1)
    {
        for (int i = 0; i < 4; i++)
        {
            cnt++;
            t[i].tid = pthread_self();
            t[i].cnt = cnt;
        }
        int writecnt = 4;
        while (writecnt > 0)
        {
            uint32_t writedown_cnt = append_queue(&qd, &t[4 - writecnt], writecnt);
            writecnt -= writedown_cnt;
            printf("append_queue cnt is %d tid is %ld \n", writedown_cnt, t[0].tid);
            usleep(1000);
        }
    }
}
//非阻塞出队
void *tast_pop_queen(void *arg)
{
    Test t;
    //  t.tid=pthread_self();
    while (1)
    {
        int8_t ret = pop_queue(&qd, &t);
        if (ret != 0)
        {
            printf("pop_queue return %d \n", ret);
        }
        else
            printf("tid is %ld, cnt is %d\n", t.tid, t.cnt);
        usleep(200);
    }
}
//阻塞出队
void *tast_wait_pop_queen(void *arg)
{
    Test t;
    // t.tid=pthread_self();
    while (1)
    {
        // cnt++;
        wait_pop_queue(&qd, &t);
        printf("block pop tid is %ld, cnt is %d\n", t.tid, t.cnt);
        usleep(400);
    }
}
//队首提取多个元素
void *tast_extract_queue(void *arg)
{
    Test t[4];
    uint32_t cnt = 0;
    while (1)
    {
        int readcnt = 4;
        while (readcnt > 0)
        {
            uint32_t readdown_cnt = extract_queue(&qd, &t[4 - readcnt], readcnt);
            readcnt -= readdown_cnt;
            printf("extract_queue cnt is %d\n", readdown_cnt);
            usleep(200);
        }
        for (int i = 0; i < 4; i++)
        {
            printf("extract tid is %ld, cnt is %d\n", t[i].tid, t[i].cnt);
        }
    }
}
Memory_Pool *mp;
void *tast_mem_pool_test1(void *arg)
{
    while (1)
    {
        void *mem1 = memory_malloc(mp, 2 * 2);
        void *mem2 = memory_malloc(mp, 18);
        void *mem3 = memory_malloc(mp, 35);

        memory_free(mp, mem2);
        void *mem4 = memory_malloc(mp, 1);
        memory_free(mp, mem3);
        if (mem4 != NULL)
            memory_free(mp, mem4);
        memory_free(mp, mem1);
        usleep(1300);
    }
}
void *tast_mem_pool_test2(void *arg)
{
    while (1)
    {
        void *mem1 = memory_malloc(mp, 2 * 2);
        void *mem2 = memory_malloc(mp, 13);
        void *mem3 = memory_malloc(mp, 33);

        memory_free(mp, mem2);
        void *mem4 = memory_malloc(mp, 21);
        memory_free(mp, mem3);
        if (mem4 != NULL)
            memory_free(mp, mem4);
        memory_free(mp, mem1);
        usleep(1100);
    }
}
void *task_write_line(void *arg)
{
    int n = 0;
    while (1)
    {
        char str[256];
        sprintf(str, "test %d\n", n);
        WriteLine("test %d %x", n, n);
        n++;
        usleep(1500);
    }
}



void *task_write_long_line(void *arg)
{
    int n = 0;
    while (1)
    {
        WriteLine( "test %d %xabcdfasdfa fewafwfe afefawf awefijoij fawefweai afwefawefo fawefjiojpj awefioawepjijf \
        fjweaopijfoiawj afweifjoiew fawoefjoiewaj awefijoi  feawiiieiiieieiieieieiei afweiiiifjeiawjfiojewoaijf aweifjoiewja aweoijfoiaiowejf\
        fewoaijfpioewajfioewja fweaf ", n, n);
        n++;
        usleep(2000);
    }
}
void *task_write_long_long_line(void *arg)
{
    int n = 0;
    while (1)
    {
        WriteLine( "test %s %s %d %xabcdfasdfa fewafwfe afefawf awefijoij fawefweai afwefawefo fawefjiojpj awefioawepjijf \
        faweoifjewopai faweoijfoepw fweaofjewopiaj fawefoijewp faweofjpoieaw afwefjawpi aweifjopij awefijopiawje awpeoifjoiawe aoweijfoi \
        fjweaopijfoiawj afweifjoiew fawoefjoiewaj awefijoi  feawiiieiiieieiieieieiei afweiiiifjeiawjfiojewoaijf aweifjoiewja aweoijfoiaiowejf\
        fewoaijfpioewajfioewja fweaf ",
                  __DATE__, __TIME__, n, n);
        n++;
        usleep(3000);
    }
}
void *print(void *pi)
{
    struct timeval us;
    int i = *(int*)pi;
    gettimeofday(&us, NULL);
    printf("id is %d gettimeofday: tv_sec=%ld, tv_usec=%ld\n", i, us.tv_sec, us.tv_usec);

}
void *task_time_wait(void *arg)
{
    int n = 0;
    Timer_t t;
    int i = 6;
    Creat_Timer(&t);
    while (1)
    {
        print(&i);

        Start_Timer(t, 10, print, (void *)&i);
        usleep(100000);
    }
}
int main(int argc, char const *argv[])
{


    printf("size of Memory_Map_Table %ld \n", sizeof(Free_Page_Info));
    init_queue(&qd, testQueen, sizeof(Test), 10);
    InitConsole();

    pthread_t thread_push, thread_wait_push, thread_pop, thread_wait_pop, thread_append, thread_extract, thread_write, thread_long_write, thread_llong_write;
    pthread_create(&thread_push, NULL, tast_push_queue, NULL);
    pthread_create(&thread_wait_push, NULL, tast_wait_push_queen, NULL);
    pthread_create(&thread_pop, NULL, tast_pop_queen, NULL);
    pthread_create(&thread_wait_pop, NULL, tast_wait_pop_queen, NULL);
    pthread_create(&thread_append, NULL, tast_append_queen, NULL);
    pthread_create(&thread_extract, NULL, tast_extract_queue, NULL);
    // pthread_t thread_mem_test1,thread_mem_test2,thread_mem_test3;
    //  mp = memory_pool_init(17, 800);

    // pthread_create(&thread_mem_test1, NULL, tast_mem_pool_test1, NULL);
    // pthread_create(&thread_mem_test2, NULL, tast_mem_pool_test2, NULL);
    // pthread_create(&thread_mem_test3, NULL, tast_mem_pool_test1, NULL);
//    pthread_create(&thread_write, NULL, task_write_line, NULL);
//    pthread_create(&thread_long_write, NULL, task_write_long_line, NULL);
//    pthread_create(&thread_llong_write, NULL, task_write_long_long_line, NULL);
    //pthread_create(&thread_out, NULL, task_out_line, NULL);
    //pthread_create(&thread_timer, NULL, task_time_wait, NULL);


    
    int j = 10;
   // Timer_Wait_t t;
    for (int i = 0; i < 20; i++)
    {
        // void *mem1 = Memory_malloc(mp, 2 * 2);
        // void *mem2 = Memory_malloc(mp, 10 * i);
        // void *mem3 = Memory_malloc(mp, 7 * i);

        // memory_free(mp, mem2);
        // void *mem4 = Memory_malloc(mp, 8 * i);
        // memory_free(mp, mem3);
        // if (mem4 != NULL)
        //     memory_free(mp, mem4);
        // memory_free(mp, mem1);

        //  MaxHeapTest();
     //   print(&j);

    //    Make_Timer(&t, 100, print, (void *)&j);
        sleep(1);
    }
    return 0;
}
//------------------------------------//
struct job
{
    struct job *j_next;
    struct job *j_prev;
    pthread_t j_id;
};
struct queue
{
    struct job *q_head;
    struct job *q_tail;
    pthread_rwlock_t q_lock;
};
int queue_init(struct queue *qp)
{
    int err;
    qp->q_head = NULL;
    qp->q_tail = NULL;
    err = pthread_rwlock_init(&qp->q_lock, NULL); //初始化读写锁
    if (err != 0)
        return err;
    return 0;
}
void job_insert(struct queue *qp, struct job *jp)
{
    pthread_rwlock_wrlock(&qp->q_lock); //写锁
    jp->j_next = qp->q_head;
    jp->j_prev = NULL;
    if (qp->q_head != NULL)
        qp->q_head->j_prev = jp;
    else
        qp->q_tail = jp;
    qp->q_head = jp;
    pthread_rwlock_unlock(&qp->q_lock); //解锁
}
void job_append(struct queue *qp, struct job *jp)
{
    pthread_rwlock_wrlock(&qp->q_lock); //写锁
    jp->j_next = NULL;
    jp->j_prev = qp->q_tail;
    if (qp->q_tail != NULL)
        qp->q_tail->j_next = jp;
    else
        qp->q_head = jp;
    qp->q_tail = jp;
    pthread_rwlock_unlock(&qp->q_lock); //解锁
}
void job_remove(struct queue *qp, struct job *jp)
{
    pthread_rwlock_wrlock(&qp->q_lock); //写锁
    if (jp == qp->q_head)
    {
        qp->q_head = jp->j_next;
        if (qp->q_tail == jp)
            qp->q_tail = NULL;
        else
            jp->j_next->j_prev = jp->j_prev;
    }
    else if (jp == qp->q_tail)
    {
        qp->q_tail = jp->j_prev;
        jp->j_prev->j_next = jp->j_next;
    }
    else
    {
        jp->j_prev->j_next = jp->j_next;
        jp->j_next->j_prev = jp->j_prev;
    }
    pthread_rwlock_unlock(&qp->q_lock); //解锁
}
struct job *job_find(struct queue *qp, pthread_t id)
{
    struct job *jp;
    if (pthread_rwlock_rdlock(&qp->q_lock) != 0) //读锁
    {
        return NULL;
    }
    for (jp = qp->q_head; jp != NULL; jp = jp->j_next)
    {
        if (pthread_equal(jp->j_id, id))
            break;
    }
    pthread_rwlock_unlock(&qp->q_lock); //解锁
    return jp;
}