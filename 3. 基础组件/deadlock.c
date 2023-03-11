#define _GNU_SOURCE
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


typedef int (*pthread_mutex_lock_t)(pthread_mutex_t *mutex);	//函数指针类型别名，pthread_mutex_lock_t可以定义函数指针变量
typedef int (*pthread_mutex_unlock_t)(pthread_mutex_t *mutex);

pthread_mutex_lock_t pthread_mutex_lock_f;	//定义一个函数指针变量
pthread_mutex_unlock_t pthread_mutex_unlock_f;

int pthread_mutex_lock(pthread_mutex_t *mutex) {
	pthread_t selfid = pthread_self();
	
	pthread_mutex_lock_f(mutex);
	printf("pthread_mutex_lock: %ld, %p\n", selfid, mutex);

} 

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	pthread_t selfid = pthread_self();
	
	pthread_mutex_unlock_f(mutex);
	printf("pthread_mutex_unlock: %ld, %p\n", selfid, mutex);
} 

//hook修改内核函数很常用
//dlsym返回一个函数指针，只要有这个出现，程序就会执行我们自己写的pthread_mutex_lock这个函数，而不是去调用系统的此函数
void init_hook(void) {
	pthread_mutex_lock_f = dlsym(RTLD_NEXT, "pthread_mutex_lock");
	pthread_mutex_unlock_f = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
}

#if 1

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex4 = PTHREAD_MUTEX_INITIALIZER;

void *thread_funcA(void *arg) {
	pthread_mutex_lock(&mutex1);
	sleep(1);
	pthread_mutex_lock(&mutex2);

	printf("thread_funcA\n");
	
	pthread_mutex_unlock(&mutex2);
	pthread_mutex_unlock(&mutex1);
}

void *thread_funcB(void *arg) {
	pthread_mutex_lock(&mutex2);
	sleep(1);
	pthread_mutex_lock(&mutex3);

	printf("thread_funcB\n");

	pthread_mutex_unlock(&mutex3);
	pthread_mutex_unlock(&mutex2);
}

void *thread_funcC(void *arg) {
	pthread_mutex_lock(&mutex3);
	sleep(1);
	pthread_mutex_lock(&mutex4);
	
	printf("thread_funcC\n");

	pthread_mutex_unlock(&mutex4);
	pthread_mutex_unlock(&mutex3);
}

void *thread_funcD(void *arg) {
	pthread_mutex_lock(&mutex4);
	sleep(1);
	pthread_mutex_lock(&mutex1);
	
	printf("thread_funcD\n");
	
	pthread_mutex_unlock(&mutex1);
	pthread_mutex_unlock(&mutex4);
}

int main() {
	init_hook();
	pthread_t tida, tidb, tidc, tidd;

	pthread_create(&tida, NULL, thread_funcA, NULL);
	pthread_create(&tidb, NULL, thread_funcB, NULL);
	pthread_create(&tidc, NULL, thread_funcC, NULL);
	pthread_create(&tidd, NULL, thread_funcD, NULL);

	pthread_join(tida, NULL);
	pthread_join(tidb, NULL);
	pthread_join(tidc, NULL);
	pthread_join(tidd, NULL);
	
	return 0;

}


#endif


