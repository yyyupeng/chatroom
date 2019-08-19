#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/types.h>

#define MAX_THREAD_NUM 6

typedef struct work
{
    void *(* process)(void *arg);   //任务函数指针
    void *arg;  //参数
    struct work *next;
}Work;

//线程池结构
typedef struct threadpoll
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    pthread_t *pthreadid;
    Work *tasks;    //任务队列
    int flag;       //标识线程池状态
    int max_thread_num; //线程池中允许的最大活动线程
    int cur_task;   //任务队列当前任务数
}Pool;

void pool_init();       //初始化线程池
int pool_add(void *(* process)(void *arg), void *arg); //添加任务
void *run(void *arg);   //工作线程
int pool_destroy();     //销毁线程池


#endif
