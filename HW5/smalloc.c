#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "smalloc.h" 

sm_container_t sm_head = {
	0,
	&sm_head, 
	&sm_head,
	0 
} ;

static 
void * 
_data (sm_container_ptr e)
{
	return ((void *) e) + sizeof(sm_container_t) ;
}

static 
void 
sm_container_split (sm_container_ptr hole, size_t size)
{
	sm_container_ptr remainder = (sm_container_ptr) (_data(hole) + size) ;

	remainder->dsize = hole->dsize - size - sizeof(sm_container_t) ;	
	remainder->status = Unused ;
	remainder->next = hole->next ;
	remainder->prev = hole ;
	hole->dsize = size ;
	hole->next->prev = remainder ;
	hole->next = remainder ;
}

static 
void * 
retain_more_memory (int size)
{
	sm_container_ptr hole ;
	int pagesize = getpagesize() ;
	int n_pages = 0 ;

	n_pages = (sizeof(sm_container_t) + size + sizeof(sm_container_t)) / pagesize  + 1 ;
	hole = (sm_container_ptr) sbrk(n_pages * pagesize) ;
	if (hole == 0x0)
		return 0x0 ;
	
	hole->status = Unused ;
	hole->dsize = n_pages * getpagesize() - sizeof(sm_container_t) ;
	return hole ;
}

void * 
smalloc (size_t size) 
{
	sm_container_ptr hole = 0x0, itr = 0x0, best_fit = 0x0;
    int best_diff = -1;

	for (itr = sm_head.next ; itr != &sm_head ; itr = itr->next) {
		if (itr->status == Busy)
			continue ;

		if (itr->dsize == size) {
			hole = itr ;
			break ; 
		}
        else if (size + sizeof(sm_container_t) < itr->dsize){
            if(best_diff == -1){
                best_diff = itr->dsize - size - sizeof(sm_container_t) ;
                hole = itr ;
                continue ;
            }
            
            if (best_diff < itr->dsize - size - sizeof(sm_container_t)){
                continue ;
            }
            else if(best_diff > itr->dsize - size - sizeof(sm_container_t)){
                best_diff = itr->dsize - size - sizeof(sm_container_t) ;
                hole = itr ;
                continue ; 
            }
        }
	}
    // find the best fit

	if (hole == 0x0) {
		hole = retain_more_memory(size) ;
		if (hole == 0x0)
			return 0x0 ;
		hole->next = &sm_head ;
		hole->prev = sm_head.prev ;
		(sm_head.prev)->next = hole ;
		sm_head.prev = hole ;
	}// if there is no fit, extend heap
	if (size < hole->dsize) 
		sm_container_split(hole, size) ;
    //if hole is bigger, split it
	hole->status = Busy ;
	return _data(hole) ;
}


void * 
srealloc(void * p, size_t newsize){
	sm_container_ptr itr ;
	for (itr = sm_head.next ; itr != &sm_head ; itr = itr->next) {
		if (p == _data(itr)) {
			itr->status = Unused ;
			break ;
		}
	} // now itr is the target container

	if(newsize == itr->dsize)
		return _data(itr) ;
	if(newsize + sizeof(sm_container_t) < itr->dsize){
		sm_container_split(itr, newsize) ;
		itr->status = Busy ;
		sfree(_data(itr->next)) ;
		return _data(itr) ;
	} //no need to expand

	long free_size = 0 ;
	sm_container_ptr temp;
	for(temp = itr->next; temp != &sm_head ; temp = temp->next){
		if(temp->status == Busy)
			break ;
		free_size += temp->dsize + sizeof(sm_container_t) ;
		if(free_size > newsize + sizeof(sm_container_t))
			break ;
		//it is enough
	}//measure the following free containers;

	if(free_size > newsize + sizeof(sm_container_t) ){ //if following free containers are big enough
		itr->next = temp->next ;
		temp->next->prev = itr ;
		itr->dsize += free_size ;
		//merge itr and the holes after it

		sm_container_split(itr, newsize) ;
		itr->status = Busy ;
		return _data(itr) ;
    }
	else{
		void * new_container = smalloc(newsize) ;
		memcpy(new_container, _data(itr), itr->dsize) ;
		sfree(_data(itr)) ;	
		return new_container ;
	} //else, allocate into other slot or retain more memory


}


void 
sfree (void * p)
{
	sm_container_ptr itr ;
	for (itr = sm_head.next ; itr != &sm_head ; itr = itr->next) {
		if (p == _data(itr)) {
			itr->status = Unused ;
			break ;
		}
	} // now itr is the sm_container to be freed


    if(itr != sm_head.next && itr->prev->status == Unused){
        itr->prev->next = itr->next ;
        itr->next->prev = itr->prev ;
        itr->prev->dsize += (itr->dsize + sizeof(sm_container_t)) ;
        itr = itr->prev ;
    }

    if(itr != sm_head.prev && itr->next->status == Unused){
        itr = itr->next ;
        itr->prev->next = itr->next ;
        itr->next->prev = itr->prev ;
        itr->prev->dsize += (itr->dsize + sizeof(sm_container_t)) ;
        itr = itr->prev ;
    }
    //merge holes if they are adjacent
	


}


void
sshrink(){
	sm_container_ptr itr ;
	long free_size = 0 ;

	for (itr = sm_head.prev ; itr != &sm_head ; itr = itr->prev) {
		if(itr->status == Busy)
			break ;
		else{
			free_size += itr->dsize + sizeof(sm_container_t) ;
			sm_head.prev = itr->prev ;
			itr->prev->next = &sm_head ;
		}
	}

	if(free_size == 0)
		return;

	sbrk(-1*free_size) ;
}


void 
print_sm_containers ()
{
	sm_container_ptr itr ;
	int i ;

	printf("==================== sm_containers ====================\n") ;
	for (itr = sm_head.next, i = 0 ; itr != &sm_head ; itr = itr->next, i++) {
		fprintf(stderr, "%3d:%p:%s:", i, _data(itr), itr->status == Unused ? "Unused" : "  Busy") ;
		fprintf(stderr, "%8d:", (int) itr->dsize) ;

		int j ;
		char * s = (char *) _data(itr) ;
		for (j = 0 ; j < (itr->dsize >= 8 ? 8 : itr->dsize) ; j++) 
			fprintf(stderr, "%02x ", s[j]) ;
		fprintf(stderr, "\n") ;
	}
	fprintf(stderr, "\n") ;

}




void
print_mem_uses()
{
    sm_container_ptr itr = 0x0 ;

    long heap_size = 0 ;
	long busy_heap = 0 ;
    long unused_heap = 0 ;

	int num_container = 0 ;
	int num_unused = 0 ;
	int num_busy = 0;

    for (itr = sm_head.next ; itr != &sm_head ; itr = itr->next) {
		num_container += 1 ;
		heap_size += itr->dsize + sizeof(sm_container_t) ;
		if(itr->status == Unused){
            unused_heap += itr->dsize + sizeof(sm_container_t) ;
			num_unused += 1 ;
        }
	}
    busy_heap = heap_size - unused_heap ;
	num_busy = num_container - num_unused ;

    fprintf(stderr, "================== Memory Use Report =================\n") ;
    fprintf(stderr, "Total allocated memory by smalloc:\n") ;
	fprintf(stderr, "%ld bytes in total\n%ld bytes for use\n", heap_size, heap_size - sizeof(sm_container_t)*num_container) ;	
    fprintf(stderr, "Total busy memory:\n") ;
	fprintf(stderr, "%ld bytes in total\n%ld bytes for use\n", busy_heap, busy_heap - sizeof(sm_container_t)*num_busy) ;
    fprintf(stderr, "Total unused memory:\n") ;
	fprintf(stderr, "%ld bytes in total\n%ld bytes for use\n\n", unused_heap, unused_heap - sizeof(sm_container_t)*num_unused) ;

}
