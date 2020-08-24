#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

int slot[8] = {0,1,2,3,4,5,6,7};

pthread_t producer_tid;
pthread_t consumer_tid[8];
int thread_count = 0; //number of threads

typedef struct {
	int path[50];
    int used[50];
    int length;
    long route_count;
    int subtask;
    
    int local_best_path[50];
    int local_best_length; 
    int local_init_length;

} path_data;

path_data** consumer_data;
path_data* producer_data;

pthread_mutex_t* local_lock;
//protects route_count, local_best...

int** m;//map
int n = 0; //number of cities

int* best_path; //minimum path found
int best_length = -1 ; //minimum length found
long total_route_count = 0;
pthread_mutex_t global_lock ;
//protects above



void path_data_init(path_data* data, int n) 
{
	for(int i=0;i<n;i++){
        data->path[i] = 0;
        data->local_best_path[i] = 0;
        data->used[i] = 0;
    }

    data->length = 0 ;
    data->route_count = 0;
    data->subtask = 0;
    
    data->local_best_length = -1;
    data->local_init_length = 0;
}

typedef struct {
	pthread_cond_t queue_cv ;
	pthread_cond_t dequeue_cv ;
	pthread_mutex_t lock ;
	path_data** elem;

    int to_read ;
	int capacity ;
	int num ; 
	int front ;
	int rear ;
} bounded_buffer ;

bounded_buffer * buf = 0x0 ;

void bounded_buffer_init(bounded_buffer * buf, int capacity)
{
	pthread_cond_init(&(buf->queue_cv), 0x0) ;
	pthread_cond_init(&(buf->dequeue_cv), 0x0) ;
	pthread_mutex_init(&(buf->lock), 0x0) ;
	buf->capacity = capacity ;

	buf->elem = malloc(sizeof(path_data*)*capacity) ;
    for(int i=0; i<capacity; i++){
        buf->elem[i] = malloc(sizeof(path_data));
        path_data_init((buf->elem[i]), n);
    }

	buf->num = 0 ;
	buf->front = 0 ;
	buf->rear = 0 ;
    buf->to_read = 0;
}
void
bounded_buffer_emergency_queue(bounded_buffer* buf, int s, int e){
    pthread_mutex_lock(&(buf->lock)) ;
    for(int i = s; i < e; i++){        
        while (buf->num == buf->capacity)
            pthread_cond_wait(&(buf->queue_cv), &(buf->lock)) ;    
        
        memcpy(buf->elem[buf->rear], consumer_data[i], sizeof(path_data));

        buf->rear = (buf->rear + 1) % buf->capacity ;
        buf->num += 1 ;
    }
    pthread_cond_signal(&(buf->dequeue_cv)) ;
	pthread_mutex_unlock(&(buf->lock)) ;
}

void 
bounded_buffer_queue(bounded_buffer * buf, path_data* item) 
{
    
    pthread_mutex_lock(&global_lock) ;
    int temp_count = thread_count;
    pthread_mutex_unlock(&global_lock) ;

	pthread_mutex_lock(&(buf->lock)) ;
        while (buf->num == temp_count)
            pthread_cond_wait(&(buf->queue_cv), &(buf->lock)) ;    
        
        memcpy(buf->elem[buf->rear], item, sizeof(path_data));

        buf->rear = (buf->rear + 1) % buf->capacity ;
        buf->num += 1 ;

        pthread_cond_signal(&(buf->dequeue_cv)) ;
	pthread_mutex_unlock(&(buf->lock)) ;

}

path_data* bounded_buffer_dequeue(bounded_buffer* buf) 
{
	path_data* item;
    item = malloc(sizeof(path_data));
    path_data_init(item, n);

	pthread_mutex_lock(&(buf->lock)) ;
        buf->to_read++;
		while (buf->num == 0) 
			pthread_cond_wait(&(buf->dequeue_cv), &(buf->lock)) ;

        memcpy(item, buf->elem[buf->front], sizeof(path_data));

		buf->front = (buf->front + 1) % buf->capacity ;
		buf->num -= 1 ;
        buf->to_read--;

	    pthread_cond_signal(&(buf->queue_cv)) ;
	pthread_mutex_unlock(&(buf->lock)) ;

	return item ;
}


void handler(int sig)
{
    pthread_mutex_lock(&global_lock) ;
        for(int i=0; i<thread_count; i++){
            pthread_mutex_lock(&local_lock[i]);
            path_data* local = consumer_data[i];
            total_route_count += local->route_count;

            if((local->local_best_length < best_length && local->local_best_length>0) || best_length<0)
            {
                best_length = local->local_best_length;
                for(int i=0; i<n; i++){
                    best_path[i] = local->local_best_path[i];
                }
            }
            pthread_mutex_unlock(&local_lock[i]);
        }
    pthread_mutex_unlock(&global_lock);


    
    printf("\n\nfinal minimum length: %d \nminimum path: (", best_length) ;
    for (int i = 0 ; i < n ; i++) 
        printf("%d ", best_path[i]) ;
    printf("%d) \nsearched routes: %ld\n", best_path[0], total_route_count) ;


    for(int i=0; i<thread_count; i++){
        pthread_cancel(consumer_tid[i]);
    }
    pthread_cancel(producer_tid);

    exit(0) ;
}

void clean_up (void* arg)
{
    int t_num = *(int*)arg;
    pthread_mutex_unlock(&buf->lock);
    pthread_mutex_unlock(&local_lock[t_num]);
    pthread_mutex_unlock(&global_lock);

    return;
}



void consumer_travel(int idx, int t_num)
{ //every first call is _travle_consumer(n-11)
	
	int i ;
    path_data* local = consumer_data[t_num];

	if (idx == n) { //last recursive call

        local->length += m[local->path[n-1]][local->path[0]] ;
        pthread_mutex_lock(&local_lock[t_num]);
            local->route_count++; 
        pthread_mutex_unlock(&local_lock[t_num]); 
        
		if (local->local_best_length < 0 || local->local_best_length > local->length) {

            pthread_mutex_lock(&local_lock[t_num]);
			local->local_best_length = local->length ;
            for (i = 0 ; i < n ; i++) 
				local->local_best_path[i] = local->path[i] ;  
            pthread_mutex_unlock(&local_lock[t_num]);         
		}

		local->length -= m[local->path[n-1]][local->path[0]] ;
	}
	else {
		for (i = 0 ; i < n ; i++) {
			if (local->used[i] == 0) {
				local->path[idx] = i ;
				local->used[i] = 1 ;
				local->length += m[local->path[idx-1]][i] ;
				consumer_travel(idx+1, t_num) ;
				local->length -= m[local->path[idx-1]][i] ;
				local->used[i] = 0;
			}
		}
	}
}

void* consumer_proc(void* arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0x0);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0x0);
    
    int t_num = *(int*) arg;

    pthread_cleanup_push(clean_up, &t_num);
  
    path_data* local = consumer_data[t_num];
    int local_best_length = -1;
    int subtask = 0;
        
    while(1)
    {//until job is all done
        pthread_mutex_lock(&local_lock[t_num]);
            memcpy(local, bounded_buffer_dequeue(buf), sizeof(path_data)) ;
            local->local_best_length = local_best_length;
            local->subtask = ++subtask ;
            local->route_count = 0;
        pthread_mutex_unlock(&local_lock[t_num]);

        consumer_travel(n-11, t_num);
  
        pthread_mutex_lock(&global_lock) ;
            total_route_count += local->route_count;
            if (best_length == -1 || best_length > local->local_best_length) {
                best_length = local->local_best_length ;
                local_best_length = local->local_best_length;

                for (int i = 0 ; i < n ; i++) 
                    best_path[i] = local->local_best_path[i] ;            
            }
        pthread_mutex_unlock(&global_lock) ;
    }

    pthread_cleanup_pop(0);

    return 0x0;
}


void producer_travel(int idx) 
{ //initial call is _travel(1)
	int i ;

	if (idx == n-11) { //last recursive call for parent
        producer_data->local_init_length = producer_data->length;
        bounded_buffer_queue(buf, producer_data);
    }
	else {
		for (i = 0 ; i < n ; i++) {
			if (producer_data->used[i] == 0) {
				producer_data->path[idx] = i ;
				producer_data->used[i] = 1 ;
				producer_data->length += m[producer_data->path[idx-1]][i] ;
				
                producer_travel(idx+1) ;
				
                producer_data->length -= m[producer_data->path[idx-1]][i] ;
				producer_data->used[i] = 0 ;
			}
		}
	}
}

void* producer_proc() 
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0x0);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0x0);

    int i;
    for(i=0; i<n; i++){
        producer_data->path[0] = i ; //set start of path with the starting city
        producer_data->used[i] = 1 ; //mark the used[start] to be used
        producer_travel(1) ; // call _travel(1)
        producer_data->used[i] = 0 ; //mark used[start] to unused
    }

    while(1){
        pthread_mutex_lock(&global_lock);
        pthread_mutex_lock(&(buf->lock));
            if(buf->to_read == thread_count){
                handler(0);
            }
        pthread_mutex_unlock(&(buf->lock));
        pthread_mutex_unlock(&global_lock);

        sleep(3);
    }


    return 0x0;
}

void stat(){
    pthread_mutex_lock(&global_lock) ;

        for(int i=0; i<thread_count; i++){
            pthread_mutex_lock(&local_lock[i]);
                path_data* local = consumer_data[i];
                total_route_count += local->route_count;

                if((local->local_best_length < best_length && local->local_best_length>0) || best_length<0)
                {
                    best_length = local->local_best_length;
                    for(int i=0; i<n; i++)
                        best_path[i] = local->local_best_path[i];
                }
            pthread_mutex_unlock(&local_lock[i]);
        }

    
        printf("\nThe best solution up to this point\n");
        printf("Shortest length: %d\n", best_length);
        printf("Shortest path: (");
        for(int i=0; i<n; i++)
            printf("%d ", best_path[i]) ;
        printf("%d) \nSearched routes: %ld\n", best_path[0], total_route_count) ;

    pthread_mutex_unlock(&global_lock);

    return;
}

void threads(){
    printf("\nThreads information\n");
    printf("Total number of consumer threads: %d\n\n", thread_count);

    for(int i=0; i<thread_count; i++){
        printf("Thread #%d\n", i);
        printf("thread ID: %ld\n", consumer_tid[i]);
        pthread_mutex_lock(&local_lock[i]);
        printf("number of subtasks completed: %d\n", consumer_data[i]->subtask-1);
        printf("number of routes checked in current subtask: %ld\n",consumer_data[i]->route_count );
        pthread_mutex_unlock(&local_lock[i]);
    }

    return;
}


void increase_thread_count(int new_thread_count){

    for(int i = thread_count; i< new_thread_count; i++){
        pthread_create(&consumer_tid[i], 0x0, consumer_proc, &slot[i]) ;
    }

    pthread_mutex_lock(&global_lock);
        thread_count = new_thread_count;
    pthread_mutex_unlock(&global_lock);

    return;
}
void decrease_thread_count(int new_thread_count){
    
    path_data* local;

    for(int i = new_thread_count; i < thread_count; i++)
        pthread_cancel(consumer_tid[i]);

    for(int i = new_thread_count; i < thread_count; i++){
            local = consumer_data[i];
            for(int j = n-11; j < n; j++)
                local->path[j]=0;
            for(int j = 0; j<n; j++)
                local->used[j] = 0;
            for(int j=0; j < n-11; j++)
                local->used[local->path[j]] = 1;
            local->length = local->local_init_length;
    }

    bounded_buffer_emergency_queue(buf, new_thread_count, thread_count);
 
    pthread_mutex_lock(&global_lock);
        thread_count = new_thread_count;
    pthread_mutex_unlock(&global_lock);

}


void control_thread_count(int new_thread_count){
    if(thread_count < new_thread_count){
        increase_thread_count(new_thread_count);
    }
    else if(thread_count > new_thread_count){
        decrease_thread_count(new_thread_count);
    }
    
    return;
}



int main(int argc, char ** args) { //receives filepath and thread_count
    if(argc != 3){
        printf("Wrong number of arguments!");
        exit(0);
    }

    signal(SIGINT, handler) ;
	int i, j, t;
    thread_count = atoi(args[2]);
 
    char buffer[200];
    FILE* fp = fopen(args[1], "r") ;
        fgets(buffer, 200, fp);
    fclose(fp);
    strtok(buffer, " ");
    while(strtok(NULL, " ") != NULL)
        n++;
    //find out number of cities

    producer_data = malloc(sizeof(path_data)) ;
    path_data_init(producer_data, n);
    consumer_data = malloc(sizeof(path_data*)*8);
    local_lock = malloc(sizeof(pthread_mutex_t)*8);
    for(int i=0; i<8; i++){
        consumer_data[i] = malloc(sizeof(path_data));
        path_data_init((consumer_data[i]), n);
        pthread_mutex_init(&local_lock[i], 0x0);
    }
    //allocate path_data's with n 
    
    
    best_path = (int* )calloc(n, sizeof(int));
    m = (int** )calloc(n, sizeof(int* ));
    for(i=0; i<n; i++){
        m[i] = (int* )calloc(n, sizeof(int));
    }
    //allocate m, best_path

    fp = fopen(args[1], "r") ;
	for (i = 0 ; i < n ; i++) {
		for (j = 0 ; j < n ; j++) {
			fscanf(fp, "%d", &t) ;
			m[i][j] = t ;
		}
	}
	fclose(fp) ;
    //read input file
    buf = malloc(sizeof(bounded_buffer)) ;
    pthread_mutex_init(&global_lock, 0x0) ;
    bounded_buffer_init(buf, 16);
    // initialize locks and bounded buffer

    pthread_create(&producer_tid, 0x0, producer_proc, 0x0) ;
    
    for(i=0; i< thread_count; i++){
        pthread_create(&consumer_tid[i], 0x0, consumer_proc, &slot[i]) ;
    }
    //initialize threads and log their id


	int count = 0 ;

    while(1){
        printf("***Choose operation***\n") ;
        printf("stat : to view the current status\n") ;
        printf("threads : to view informations of running threads\n") ;
        printf("num N (1<= N <= 8): to adjust the number of running threads\n") ;
        printf("Your choice is : ") ;

        scanf("%s", buffer) ;

        if( strcmp(buffer,"stat") == 0 ){
          stat();
        }

        else if( strcmp(buffer,"threads") == 0 ){
            threads();
        }

        else if(strcmp(buffer,"num") == 0 ){
            int new_thread_count;
            scanf("%d", &new_thread_count);

            control_thread_count(new_thread_count);
            
        }
        else {
            printf("Your input is '%s'\nWrong input! Try again\n", buffer) ;            
        }

        putchar('\n') ;
        continue ;
        
    }

    


    printf("\nfinal minimum length: %d \nminimum path: (", best_length) ;
    for (i = 0 ; i < n ; i++) 
        printf("%d ", best_path[i]) ;
    printf("%d) \nsearched routes: %ld\n", best_path[0], total_route_count) ;
    //print the results

    return 0;
}