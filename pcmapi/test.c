#include "test.h"
#include "p_mmap.h"
#include "stdlib.h"

typedef struct
{
	int data;
	int next;
}LinkedNode;

int main(int argc, char **argv) {
    
    int iRet = 0;
    char *ptr = NULL;

    printf("ready to call p_init\n");
    iRet = p_init();
    if (iRet < 0) {
        printf("error: p_init\n");
        return -1;
    }

    printf("%d %s\n",argc,argv[1]);
    if (argc == 2 && argv[1][0] == 'c') {
        printf("ready to clear\n");
        iRet = p_clear();
        if (iRet < 0) {
            printf("error: p_clear\n");
            return -1;
        }

        return 0;
    }
    else if (argc == 3 && argv[1][0] == 'w') { //write the linked list
    	int i;
    	int t;
    	char* base=(char*)p_get_base();
    	LinkedNode* nd,*last;
    	t==atoi(argv[2]);
    	last=(LinkedNode*)p_malloc(sizeof(LinkedNode));
		last->data=-1;
		p_bind(1234,last,sizeof(LinkedNode));
    	for(i=0;i<t;i++)
    	{
        	nd=(LinkedNode*)p_malloc(sizeof(LinkedNode));
    		nd->data=i;
    		last->next=(char*)nd-base;
    		last=nd;
    	}
    	last->next=0;
    }
    else if (argc == 2 && argv[1][0] == 'r') { //read and check the linked list
    	char* base=(char*)p_get_base();
    	int sz,i;
    	LinkedNode* nd=p_get_bind_node(1234,&sz);
    	printf("First Node ptr=%llx sz=%d\n",nd,sz);
    	i=-1;
    	for(;;)
    	{
        	if(nd->data!=i)
        	{
        		printf("Check Error! data=%d i=%d\n",nd->data,i);
        		break;
        	}
        	i++;
    		if(nd->next)
    			nd=(LinkedNode*)(base+nd->next);
    		else
    			break;
    	}
    	printf("Check finish! i=%d\n",i);
    }
    else
    	printf("No op\n");
 /*
    ptr = (char *)p_malloc(1);
    printf("return from p_malloc 1, addr=%p\n", ptr);
    
    p_free(ptr, 1);
    printf("after free %p\n", ptr);

    ptr = (char *)p_malloc(1);
    printf("return from p_malloc 1, addr=%p\n", ptr);

    ptr = (char *)p_malloc(1);
    printf("return from p_malloc 1, addr=%p\n", ptr);

    ptr = (char *)p_malloc(10);
    printf("return from p_malloc 10, addr=%p\n", ptr);

    ptr = (char *)p_malloc(100);
    printf("return from p_malloc 100, addr=%p\n", ptr);

    p_free(ptr, 100);
    printf("after free %p\n", ptr);

    ptr = (char *)p_malloc(100);
    printf("return from p_malloc 100, addr=%p\n", ptr);
    */
    /*
    printf("ready to call p_get\n");
    ptr = p_get(23, 4096);
    if (!ptr) {
        printf("ready to call p_new\n");
        ptr = p_new(23, 4096);
        if (!ptr) {
            printf("p_new failed\n");
            return -1;
        }
    }

    
    int *tmp = (int *)ptr;
    printf("=== ptr = %p, value = %d, ready to add 1\n", ptr,*tmp);
    *tmp += 1;
    */
    return 0;
}

