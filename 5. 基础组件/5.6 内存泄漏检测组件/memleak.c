

#define _GNU_SOURCE
#include <dlfcn.h>



#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <mcheck.h>


#if 0

void *_malloc(size_t size, const char *filename, int line) {

	void *p = malloc(size);

	char buff[128] = {0};
	sprintf(buff, "./mem/%p.mem", p);

	FILE *fp = fopen(buff, "w");
	fprintf(fp, "[+]%s:%d, addr: %p, size: %ld\n", 
		filename, line, p, size);

	fflush(fp);
	fclose(fp);
	//printf("[+]%s:%d, %p\n", filename, line, p);

	return p;

}

void _free(void *p, const char *filename, int line) {

	//printf("[-]%s:%d, %p\n", filename, line, p);

	char buff[128] = {0};
	sprintf(buff, "./mem/%p.mem", p);

	if (unlink(buff) < 0) {
		printf("double free: %p\n", p);
		return ;
	}
	
	return free(p);

}

#define malloc(size)   _malloc(size, __FILE__, __LINE__)

#define free(size)   _free(size, __FILE__, __LINE__)

#else

typedef void *(*malloc_t)(size_t size);
typedef void (*free_t)(void *ptr);

malloc_t malloc_f = NULL;
free_t free_f = NULL;

void *malloc(size_t size) {

	printf("size: %ld\n", size);

	return NULL;
}


void free(void *ptr) {

	printf("ptr: %p\n", ptr);

}




static void init_hook(void) {

	if (malloc_f == NULL) {
		malloc_f = (malloc_t)dlsym(RTLD_NEXT, "malloc");
	}

	
	if (free_f == NULL) {
		free_f = (free_t)dlsym(RTLD_NEXT, "free");
	}

}




#endif


// export MALLOC_TRACE=./mem.txt
int main() {

	//mtrace();
	//setenv("MALLOC_TRACE", "./mem.txt");

	init_hook();

	void *p1 = malloc(10);  //_malloc(size, __FILE__, __LINE__)
	void *p2 = malloc(15);
	void *p3 = malloc(20);

	free(p2);
	free(p3);

	//muntrace();

}



