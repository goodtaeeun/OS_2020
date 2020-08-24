#include <stdio.h>
#include "smalloc.h"


int 
main()
{
	void *p1, *p2, *p3, *p4 ;

    p1 = smalloc(2000) ; 
	printf("smalloc(2000):%p\n", p1) ; 
    
	p2 = smalloc(2500) ; 
	printf("smalloc(2500):%p\n", p2) ;

	p3 = smalloc(1532) ; 
	printf("smalloc(1532):%p\n", p3) ; 

	p4 = smalloc(2032) ; 
	printf("smalloc(2032):%p\n\n", p4) ; 

    print_sm_containers() ;

    sfree(p1) ; 
	printf("sfree(%p)\n", p1) ; 

    sfree(p2) ; 
	printf("sfree(%p)\n", p2) ; 
    
    sfree(p3) ; 
	printf("sfree(%p)\n", p3) ; 

    sfree(p4) ; 
	printf("sfree(%p)\n\n", p4) ;

    print_sm_containers() ;

    
    //sshrink();
    //print_mem_uses() ;
    //use only with smalloc 2.0



}
