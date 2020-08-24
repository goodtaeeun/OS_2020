#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <error.h>
#include <string.h>

char user_name[128] ;
char file_name[128] ;
int pipes[2] ;

void
child_proc()
{
    char buf[32] ;

	dup2(pipes[1], 1/*standard output*/) ;
	sprintf(buf, "id -u %s", user_name) ;
	system(buf) ;//now uid is written to pipes[1]
	
	exit(0) ;
	
}

uid_t
parent_proc()
{
	char buf[32] ;
	ssize_t s ;

	close(pipes[1]) ;

	s = read(pipes[0], buf, 31) ;
	buf[s + 1] = 0x0 ;	
    
	return atoi(buf) ;
}



uid_t
get_uid()
{
	uid_t target_uid ;
    pid_t child_pid ;
	int exit_code ;

	if (pipe(pipes) != 0) {
		perror("Error") ;
		return 1 ;
	}
	
	child_pid = fork() ;
	if (child_pid == 0) {
		child_proc() ;
	}
	else {
		target_uid = parent_proc() ;
	}
	wait(&exit_code) ;

	return target_uid ;
}


int 
main() 
{	
    char op ;
    char buffer[128] ;
	int count = 0 ;

    while(1){
        printf("***Choose operation***\n") ;
        printf("b : to block a certain user from opening some files\n") ;
        printf("i : to give immortality to processes owned by certain user\n") ;
        printf("s : to see the status\n") ;
        printf("d : to set to default settings.\n") ;
        printf("q : to quit and exit\n\n") ;
        printf("Your choice is : ") ;

        scanf("%c", &op) ;
	    if(op=='\n') op = getchar() ;

        if(op == 'b'){
            printf("Enter the name of user you want to block : ") ;
            scanf("%s", user_name) ;
            printf("Enter part of the file name you want to block : ") ;
            scanf("%s", file_name) ;
            uid_t uid = get_uid() ;

            pid_t child_pid ;
            child_pid = fork() ;
            if (child_pid == 0) {
                int fd = open("/proc/mousehole", O_WRONLY, 0644) ;
                dup2(fd, 1 /* STDOUT*/) ;
                
                sprintf(buffer, "b %d %s ", uid, file_name) ;
		        int s = strlen(buffer) ;
        		ssize_t sent = 0 ;
		
                char* data = buffer ;
                while (sent < s) {
                sent += write(fd, data + sent, s - sent) ;
                }
                close(fd) ;
                return 0 ;
            }
            wait(0x0) ;
            
        }

        else if(op=='i'){
            printf("Enter the name of user you want to give immortality : ") ;
            scanf("%s", user_name) ;
            uid_t uid = get_uid() ;
            
            pid_t child_pid ;        
            child_pid = fork() ;
            if (child_pid == 0) {
                int fd = open("/proc/mousehole", O_RDWR, 0666) ;
                dup2(fd, 1 /* STDOUT*/) ;
                close(fd) ;//now writing to stdout is writing to /proc/mousehole
                
                printf("i %d ",uid) ;
                return 0 ;
            }
            wait(0x0) ;
        }

        else if(op == 's'){
            system("cat /proc/mousehole");
        }

        else if(op == 'q'){
            printf("Bye Bye!\n") ;
            return 0 ;
        }

	else if(op=='d'){
            pid_t child_pid ;        
	    
            child_pid = fork() ;
            if (child_pid == 0) {
                int fd = open("/proc/mousehole", O_RDWR, 0666) ;
                dup2(fd, 1 /* STDOUT*/) ;
                close(fd) ;//now writing to stdout is writing to /proc/mousehole
                printf("d") ;
                return 0 ;
            }
            wait(0x0) ;

	}

        else {
            printf("Your input is '%c'\nWrong input! Try again\n", op) ;            
        }

        putchar('\n') ;
        continue ;
        
    }

    return 0;
}
