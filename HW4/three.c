#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;


void 
noise()
{
	usleep(rand() % 1000) ;
}

void *
thread1(void *arg) 
{
	noise() ;
		pthread_mutex_lock(&mutex);
		pthread_mutex_lock(&mutex1);	 //noise() ;
		noise();
		pthread_mutex_lock(&mutex2); //noise() ;
		pthread_mutex_unlock(&mutex2); //noise() ;
		pthread_mutex_unlock(&mutex1); //noise() ;
		pthread_mutex_unlock(&mutex);

		return NULL;
}

void *
thread2(void *arg) 
{
	noise();
		pthread_mutex_lock(&mutex2);	 //noise() ;
		sleep(1);
		pthread_mutex_lock(&mutex3); //noise() ;
		pthread_mutex_unlock(&mutex3); //noise() ;
		pthread_mutex_unlock(&mutex2); //noise() ;

		return NULL;
}


int 
main(int argc, char *argv[]) 
{
	pthread_t tid1, tid2;
	srand(time(0x0)) ;


	pthread_create(&tid1, NULL, thread1, NULL);
	pthread_create(&tid2, NULL, thread2, NULL);
		
	noise();
	pthread_mutex_lock(&mutex3);// noise() ; 
	noise();
	pthread_mutex_lock(&mutex1);	//noise() ; 
	pthread_mutex_unlock(&mutex1); //noise() ;
	pthread_mutex_unlock(&mutex3); //noise() ;

	//pthread_join(tid, NULL);
	return 0;
}

