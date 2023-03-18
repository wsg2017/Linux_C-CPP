#ifndef _THREAD_POOL_H 
#define _THREAD_POOL_H

typedef struct thread_pool_t thread_pool_t;     //隐藏实现，只把声明暴露给用户
typedef void (*handler_pt) (void *);    //函数指针类型定义的通用写法

thread_pool_t *thread_pool_create(int thrd_count, int queue_size);

int thread_pool_post(thread_pool_t *pool, handler_pt func, void *arg);  //抛出执行任务

int thread_pool_destroy(thread_pool_t *pool);

int wait_all_done(thread_pool_t *pool);

#endif