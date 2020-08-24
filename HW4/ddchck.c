#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

int mutex_map[10][10];

int peak;

pthread_mutex_t* mutex_list[10];
int mutex_count = 0;

int stack[20];
int top = -1;
void stack_init(){
    top = -1;
}
void push(int elem){
    stack[++top] = elem;
}
int pop(){
    return stack[top--];
}
// stack[top] is the top element
// if top is -1, it is empty


typedef struct Node{
    int mutex_index;
    pthread_t mutex_tid ;
    struct Node* next;
} Node;
//data structure to implement nodes in the linked list.
Node node_list[10]; //고정된 mutex 정보 리스트
Node* adj_list[10]; // 매번 새로 생성되는 adjacency list

Node* create_node(int index){ // adj 생성을 위한 함수, 원본 mutex data를 복사해서 생성
    Node* newNode = (Node* )malloc(sizeof(Node));

    memcpy(newNode, &node_list[index], sizeof(Node)) ;
    newNode->next = NULL;

    return newNode;
}

int what_is_index(pthread_t tid, pthread_mutex_t* m, int mode){ // mutex의 index 알려줌, 새로운 mutex면 새로운 index알려줌
    for(int i=0; i< mutex_count; i++){ //전에 본 mutex인 경우
        if(m == mutex_list[i]){
            if(node_list[i].mutex_tid == 0 )
                node_list[i].mutex_tid = tid; // 주인이 없었다면 새로 점유
            return i;
        }

    }

    if(mode == 1)
        return -1; // 잡지도 않은 걸 unlock할 때

    mutex_list[mutex_count] = m;
    //node_list[mutex_count].mutex_index = mutex_count ;
    node_list[mutex_count].mutex_tid = tid;

    return mutex_count++; // if a new one
}

void unlock_mutex(pthread_t tid, int index){ //release될 mutex와 연결된 모든 edge사라짐
    for(int i=0; i<mutex_count; i++){
        mutex_map[i][index] = 0 ;
        mutex_map[index][i] = 0 ;
    } 
}

void lock_mutex(pthread_t tid, int index){
    for(int i=0; i<mutex_count; i++){
        if(node_list[i].mutex_tid == tid){ // 해당 mutex의 tid가 새로운 mutex의 tid와 같다면
            mutex_map[i][index] = 1 ; //표시하삼        
        }
        mutex_map[i][i] = 0;
    }
}


void make_adj(){
    for(int i=0; i<mutex_count; i++){
        adj_list[i] = create_node(i);
        Node* temp = adj_list[i];
        for(int j=0; j<mutex_count; j++){
            if(mutex_map[i][j] == 1){
                temp->next = create_node(j) ;
                temp = temp->next ;
            }
        }
    } // make adjacency list
}


int dfs(int index, int* status){

    if(status[index] == 0){
		status[index] = 1;
        push(index);
    }

    if(adj_list[index] == NULL)
        return 0;
    
	adj_list[index] = adj_list[index]->next ;
    if(adj_list[index] == NULL){ //만약 이제 backtrack해야 한다면
        status[index] = -1;
        return 0;
    }

    int next_index = adj_list[index]->mutex_index ;
    int result;
    if( status[next_index] == 0){ // 새로운 node
        result = dfs(next_index, status);
    }
    else if(status[next_index] == -1){ // 이미 끝난 노드, 뛰어넘고 계속
        result = dfs(index, status);
    }
    else if(status[next_index] == 1){ 
        peak = next_index; //돌아온 index.
        return 1;
    }

    if(result == 1)
        return 1;
    else return dfs(index, status);

    
}


int is_cycle(){
    stack_init();
    int status[10] = {0,}; // 0 is white, 1 is grey, -1 is black
    int result = 0;
    for(int i=0; i < mutex_count; i++){
        if(status[i] == -1) //이미 끝난 node 라면
            continue ;

        result = dfs(i, status); 
        if(result == 1) return 1;
    }

    return 0;
}



void lock(pthread_t tid, pthread_mutex_t* mutex){
    int index = what_is_index(tid, mutex, 0);
    //새로운 mutex인지 확인 -> 새로운 노드 만들기
    lock_mutex(tid, index);
    //lock_mutex 통해 어레이에 표시 = 해당 thread가 가장 최근에 lock한 mutex->새로운 mutex edge 생성

    make_adj();
    // adj 리스트 만들기    
    int deadlock = is_cycle();
    //DFS통해 cycle 찾기

    if(deadlock == 1){
        int mutex_in_deadlock[10];
        pthread_t threads_in_deadlock[10];
        int m = 0 ;
        int t = 0;
        int elem;
        while(stack[top] != peak){
            mutex_in_deadlock[m++] = pop();
        }
        mutex_in_deadlock[m++] = pop();
        for(int i=0; i<m; i++){
            int dup = 0;
            elem = mutex_in_deadlock[i];
            for(int j =0; j<t; j++){
                if(threads_in_deadlock[j] == node_list[elem].mutex_tid){
                    dup = 1;
                    break;
                }
            }
            if(dup == 0){
                threads_in_deadlock[t++] = node_list[elem].mutex_tid;
            }
        }

        printf("Deadlock detected!!\n");
        printf("Mutexes involved:\n");
        for(int i=0; i<m; i++){
            printf("%p\n", mutex_list[mutex_in_deadlock[i]]);
        }
        printf("Threads involved:\n");
        for(int i=0; i<t; i++){
            printf("%ld\n", threads_in_deadlock[i]);
        }

    }
    //맞다면 출력

}

void unlock(pthread_t tid, pthread_mutex_t* mutex){
    int index = what_is_index(tid, mutex, 1);
    if(index == -1)
        return; // 쓸데없는 unlock인 경우
    //mutex id 찾기
    node_list[index].mutex_tid = 0;
    //node_list에서 tid 지우기
    unlock_mutex(tid, index);
    //mutex_unlock 돌리고
    return;
    //종료
    //
    
}


int 
main (int argc, char ** args)
{
	int input_flag;
  	pthread_t input_tid;
  	pthread_mutex_t* input_mutex;

	for(int i=0; i<10; i++){
        node_list[i].mutex_index = i;
        node_list[i].mutex_tid = 0;
        node_list[i].next = NULL;
    }

	int fd = open(".ddtrace", O_RDWR | O_SYNC) ;

	while (1) {
		char s[128] ;
		int len ;
		len = read(fd, s, 128) ;

		if (len > 0){ 
            //fprintf(stderr, "%s\n",s);
			sscanf(s, "%d %ld %p", &input_flag, &input_tid, &input_mutex) ;
            
            if(input_flag==0){ //lock
                lock(input_tid, input_mutex);
            }
            else if(input_flag==1){ //unlock
                unlock(input_tid, input_mutex);
            }
            
        }
        
	}
	close(fd) ;
	return 0 ;
}
