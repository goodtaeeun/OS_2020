#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/unistd.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");

char fname[128] = { 0x0, } ;
uid_t uname = -1 ;
uid_t immortal = -1 ;

void ** sctable ;

asmlinkage int (*orig_sys_open)(const char __user * filename, int flags, umode_t mode) ; 

asmlinkage int mousehole_sys_open(const char __user * filename, int flags, umode_t mode)
{
	const struct cred *cred_now = current_cred() ;
	uid_t uid = (cred_now->uid).val ;
	//recieve current user's uid
	char file_now[256] ;
	copy_from_user(file_now, filename, 256) ;
	//receive the file that user wants to open

	if (fname[0] != 0x0 && strstr(file_now, fname) != NULL) {//check if current file conatains the target string
		if(uid == uname)//check the uid
        	return -1 ;//-EBADF;//means it was invalid file descripter, which is similar problem.
	}

	return orig_sys_open(filename, flags, mode) ;
}

asmlinkage int (*orig_sys_kill)(pid_t pid, int sig) ; 

asmlinkage int mousehole_sys_kill(pid_t pid, int sig)
{
	struct task_struct *task ;
//	task = find_task_by_vpid(pid);
//
//	if(immortal == task_uid(task).val){
//		printk("target pid is %d, target uid is %d", pid, immortal);
//		return -1;
//	}
// alternatives of finding task of given pid

	for_each_process(task) { //finding the task of given pid
		if(pid == task->pid){
			if( immortal == task_uid(task).val){
				printk("target pid is %d, target uid is %d", pid, immortal) ;
				return 0 ;
			}
		}
	}
	return orig_sys_kill(pid, sig) ;
}

static 
int mousehole_proc_open(struct inode *inode, struct file *file) {
	return 0 ;
}

static 
int mousehole_proc_release(struct inode *inode, struct file *file) {
	return 0 ;
}

static
ssize_t mousehole_proc_read(struct file *file, char __user *ubuf, size_t size, loff_t *offset) 
{
	char buf[256] ;
	ssize_t toread ;

	sprintf(buf, "blocked uid: %d\nblocked file: '%s'\nimmortal uid: %d\n", uname, fname, immortal) ;

	toread = strlen(buf) >= *offset + size ? size : strlen(buf) - *offset ;

	if (copy_to_user(ubuf, buf + *offset, toread))
		return -EFAULT ;	

	*offset = *offset + toread ;

	return toread ;
}

static 
ssize_t mousehole_proc_write(struct file *file, const char __user *ubuf, size_t size, loff_t *offset) 
{
    //jerry will send "opcode user_name file_name(optional) "
	char buf[256] ;
    char op;

	if (*offset != 0 || size > 256)
		return -EFAULT ;

	if (copy_from_user(buf, ubuf, size))
		return -EFAULT ;

    if(buf[0] == 'b'){
        sscanf(buf,"%c %d %s ", &op, &uname, fname) ;
    }
    else if(buf[0] == 'i'){
        sscanf(buf,"%c %d ", &op, &immortal) ;
    }
    else if(buf[0] == 'd'){
	uname = -1;
	fname[0] = '\0';
	immortal = -1;
    }

	//taking inputs from jerry.

	
	
	*offset = strlen(buf) ;

	return *offset ;
}

static const struct file_operations mousehole_fops = {
	.owner = 	THIS_MODULE,
	.open = 	mousehole_proc_open,
	.read = 	mousehole_proc_read,
	.write = 	mousehole_proc_write,
	.llseek = 	seq_lseek,
	.release = 	mousehole_proc_release,
} ;

static 
int __init mousehole_init(void) {
	unsigned int level ; 
	pte_t * pte ;

	proc_create("mousehole", S_IRUGO | S_IWUGO, NULL, &mousehole_fops) ;

	sctable = (void *) kallsyms_lookup_name("sys_call_table") ;

	orig_sys_open = sctable[__NR_open] ;
    orig_sys_kill = sctable[__NR_kill] ;

	pte = lookup_address((unsigned long) sctable, &level) ;
	if (pte->pte &~ _PAGE_RW) 
		pte->pte |= _PAGE_RW ;	

	sctable[__NR_open] = mousehole_sys_open ;
    sctable[__NR_kill] = mousehole_sys_kill ;

	return 0;
}

static 
void __exit mousehole_exit(void) {
	unsigned int level ;
	pte_t * pte ;
	remove_proc_entry("mousehole", NULL) ;

	sctable[__NR_open] = orig_sys_open ;
    sctable[__NR_kill] = orig_sys_kill ;
	pte = lookup_address((unsigned long) sctable, &level) ;
	pte->pte = pte->pte &~ _PAGE_RW ;
}

module_init(mousehole_init);
module_exit(mousehole_exit);
