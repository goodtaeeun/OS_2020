#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int pipes[2];
pid_t child_pid;

int** m;
int* path;//current path
int* min_path; //minimum path found
int* used ; //to mark the used city
int n = 0; //number of cities

int length = 0 ; //current length
int min = -1 ; //minimum length found
int max_child = 0 ;//maximum number of child
int child_count = 0; //current number of child
long route_count = 0;

void
handler(int sig)
{
    if(child_pid !=0){//for parent
        kill(0, SIGINT); // all child killed
        char data[200] ;
        char buf[32] ;
        ssize_t s ;
        int temp_min;
        char temp_path[200];
        long temp_route_count;


        for(int i=1; wait(0x0) != -1 ; i++){

            data[0] = 0x0;
            read(pipes[0], data, 200);
    
            sscanf(data, "%d %s %ld", &temp_min, temp_path, &temp_route_count);
            route_count +=temp_route_count;//update route_count

            if(temp_min < min || min == -1){ // if we need to replace data
               //tokenize temp_path by ,
                char *ptr = strtok(temp_path, ",");    
                for(int i=0; i<n; i++){
                    min_path[i] = atoi(ptr);
                    ptr = strtok(NULL, ",");
                }//path updated
                min = temp_min; // min updated
            }//updated the data
        }
        printf("\nfinal minimum length: %d\nminimum path: (", min) ;
        for (int i = 0 ; i < n ; i++) 
            printf("%d ", min_path[i]) ;
        printf("%d)\nsearched routes: %ld\n", min_path[0], route_count) ;
    }
    else{ //for child
        char num_buf[10];
        char route[200] ;
        char data[200];//the string to give (conists of length, route and numbers it checked)
        
        for(int i=0; i<n; i++){
            sprintf(num_buf,"%d,", min_path[i]);
            strcat(route, num_buf);
        }//write the route into a string
        sprintf(data, "%d %s %ld", min, route, route_count);// write lentgh in buffer
        //data prepared (min route route_count)

        write(pipes[1], data, 200);

    }
    
    exit(0) ;
}

void _travel_child(int idx) { //every first call is _travel_child(n-12)
	int i ;

	if (idx == n) { //last recursive call
		route_count++;
        length += m[path[n-1]][path[0]] ;
		if (min == -1 || min > length) {
			min = length ;

            for (i = 0 ; i < n ; i++) 
				min_path[i] = path[i] ;            
		}
		length -= m[path[n-1]][path[0]] ;
	}
	else {
		for (i = 0 ; i < n ; i++) {
			if (used[i] == 0) {
				path[idx] = i ;
				used[i] = 1 ;
				length += m[path[idx-1]][i] ;
				_travel_child(idx+1) ;
				length -= m[path[idx-1]][i] ;
				used[i] = 0;
			}
		}
	}
}

void child_proc(){
    route_count=0;
    _travel_child(n-12);

    char buf[32];
    char route[200] ;
    char data[200];//the string to give (conists of length, route and numbers it checked)
    
    for(int i=0; i<n; i++){
        sprintf(buf,"%d,", min_path[i]);
        strcat(route, buf);
    }//convert the path into string
    sprintf(data, "%d %s %ld", min, route, route_count);// write lentgh in buffer
    //data prepared (min route route_count)
    write(pipes[1], data, 200);

    close(pipes[0]);
    close(pipes[1]);

    exit(0);

}


void _travel_parent(int idx) { //initial call is _travel(1)
	int i ;

	if (idx == n-12) { //last recursive call for parent
          if(child_count== max_child) {//if we can't fork now

            wait(0x0); //wait for a child to terminate
            char data[200] ;
            char buf[32] ;
            ssize_t s ;
            
            read(pipes[0], data, 200);            

            int temp_min;
            char temp_path[200];
            long temp_route_count;
            sscanf(data, "%d %s %ld", &temp_min, temp_path, &temp_route_count);
            route_count+= temp_route_count;

            if(temp_min < min || min == -1){ // if we need to replace data
               //tokenize temp_path by ,
                char *ptr = strtok(temp_path, ",");    
                for(int i=0; i<n; i++){
                    min_path[i] = atoi(ptr);
                    ptr = strtok(NULL, ",");
                }//path updated
                min = temp_min; // min updated
            }//updated the data
        
            child_count--;
        } 
        //move on
        
        child_pid = fork();
        if (child_pid == 0) {
		     child_proc();
	      } //fork
        child_count++;
	}
	else {
		for (i = 0 ; i < n ; i++) {
			if (used[i] == 0) {
				path[idx] = i ;
				used[i] = 1 ;
				length += m[path[idx-1]][i] ;
				_travel_parent(idx+1) ;
				length -= m[path[idx-1]][i] ;
				used[i] = 0 ;
			}
		}
	}
}

void travel_parent(int start) {
	path[0] = start ; //set start of path with the starting city
	used[start] = 1 ; //mark the used[start] to be used
	_travel_parent(1) ; // call _travel(1)
	used[start] = 0 ; //mark used[start] to unused
}

int main(int argc, char ** args) { //receives filepath and max_child
    if(argc != 3){
        printf("Wrong number of arguments!");
        exit(0);
    }

    signal(SIGINT, handler) ;

	int i, j, t;

    max_child = atoi(args[2]);
    char buf[200];
    FILE* fp = fopen(args[1], "r") ;
        fgets(buf, 200, fp);
    fclose(fp);
    strtok(buf, " ");
    while(strtok(NULL, " ") != NULL)
        n++;
    //find out number of cities

    
    //allocate m, path, used
    path = (int* )malloc(sizeof(int)*n);
    min_path = (int* )malloc(sizeof(int)*n);
    used = (int* )malloc(sizeof(int)*n);
    m = (int** )malloc(sizeof(int* )*n);
    for(i=0; i<n; i++){
        m[i] = (int* )malloc(sizeof(int)*n);
        used[i] = 0;
    }

    fp = fopen(args[1], "r") ;
	for (i = 0 ; i < n ; i++) {
		for (j = 0 ; j < n ; j++) {
			fscanf(fp, "%d", &t) ;
			m[i][j] = t ;
		}
	}
	fclose(fp) ; //read input file //move this whole block upper than allocation part.

    if (pipe(pipes) != 0) {
		perror("Error") ;
		exit(1) ;
	}//open pipe

	for (i = 0  ; i < n ; i++) 
		travel_parent(i) ; //call travel for every starting city

    printf("\nfinal minimum length: %d \nminimum path: (", min) ;
    for (i = 0 ; i < n ; i++) 
        printf("%d ", min_path[i]) ;
    printf("%d) \nsearched routes: %ld", min_path[0], route_count) ;
    //print the results

    return 0;
}
