#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>



int 
pthread_mutex_lock (pthread_mutex_t* m)
{
    static __thread int n_lock = 0 ;
	n_lock += 1 ;

    char * error ;

	int (*pthread_mutex_lockp)(pthread_mutex_t* m) ; 	
	pthread_mutex_lockp = dlsym(RTLD_NEXT, "pthread_mutex_lock") ;
	if ((error = dlerror()) != 0x0) 
		exit(1) ;

    char s[128] ;
    pthread_t me = pthread_self();
    sprintf(s, "0 %ld %p", me, m);
    //write on s the thread number, the address of the lock
    // "0 thread_id lock_addr"

    if (n_lock == 1) { // 이 안에서의 lock이 무한 recursion 안 빠지게 보호
        int fifo = open(".ddtrace", O_WRONLY | O_SYNC) ;
        write(fifo, s, 128) ;
        close(fifo);		
	}
    
    
    int result = pthread_mutex_lockp(m) ;
    n_lock -= 1 ;

    return result ; 	
}

int 
pthread_mutex_unlock (pthread_mutex_t* m)
{
    static __thread int n_unlock = 0 ;
	n_unlock += 1 ;

	int (*pthread_mutex_unlockp)(pthread_mutex_t* m) ; 
	char * error ;
	
	pthread_mutex_unlockp = dlsym(RTLD_NEXT, "pthread_mutex_unlock") ;
	if ((error = dlerror()) != 0x0) 
		exit(1) ;

	int result = pthread_mutex_unlockp(m) ;

    char s[128] ;
    pthread_t me = pthread_self();
    sprintf(s, "1 %ld %p", me, m);
    //write on s the thread number, the address of the lock
    //"1 thread_id lock_addr"

    if (n_unlock == 1) {
        int fifo = open(".ddtrace", O_WRONLY | O_SYNC) ;
        write(fifo, s, 128) ;
        close(fifo);
	}
	n_unlock -= 1 ;

	return result ; 	
}
