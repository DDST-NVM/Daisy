#include "p_mmap.h"

static int SHM_SIZE = 0;

static char *pBaseAddr = NULL;
static int iBitsCount = 0;

static void* p_mmap(void* addr,unsigned long len,unsigned long prot,unsigned long id) {
	return (void*)syscall(__NR_p_mmap, addr, len, prot, id);
}

static int p_search_big_region_node(unsigned long id) {
	return (int)syscall(__NR_p_search_big_region_node, id);
}

static int p_delete_big_region_node(unsigned long id) {
	return (int)syscall(__NR_p_delete_big_region_node, id);
}

static int p_alloc_and_insert(unsigned long id, int size) {
    return (int)syscall(__NR_p_alloc_and_insert, id, size);
}

static int p_get_small_region(unsigned long id,unsigned long size) {
	return (int)syscall(__NR_p_get_small_region, id,size);
}

static int p_search_small_region_node(unsigned long id, void *poffset, void *psize) {
	return (int)syscall(__NR_p_search_small_region_node, id, poffset, psize);
}

static int p_bind_(unsigned long id, unsigned long offset, unsigned long size, unsigned long hptable_id) {
    return (int)syscall(__NR_p_bind, id, offset, size, hptable_id);
}


#define HPID    234567
/*
int p_init() {
    key_t key;
    int shmid;
    int mode;
    int iRet = 0;

    if ((key = ftok("/bin", 'R')) == -1) {
        perror("ftok");
        exit(1);
    }

    if ((shmid = shmget(key, SHM_SIZE, 0777 | IPC_CREAT)) == -1) {
        perror("shmget");
        exit(1);
    }

    pAddr = shmat(shmid, (void *)0, 0);
    if (pAddr == (char *)(-1)) {
        perror("shmat");
        exit(1);
    }

    printf("get and attach succee! addr=%p\n", pAddr);

    return 0;
}
*/

int p_init(int size) {
    int iRet = 0;

    if (pBaseAddr != NULL) {
        return -1;
    }

    if (size < 0) {
        return -1;
    }

    SHM_SIZE = size;
    iBitsCount = (SHM_SIZE) / 9 * 8;
    /*
    这个函数将获得该程序的inode，拼接出id，然后查找table；如果发现了，则直接映射上来，
    否则，分配一块大的区域，清0，然后映射上来
    */
    iRet = p_get_small_region(HPID,size);
    if (iRet < 0) {
        printf("error: p_get_small_region\n");
        return -1;
    }

    pBaseAddr = p_mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, HPID);
    if (!pBaseAddr) {
        printf("p_mmap return NULL\n");
    }

    int *magic_ptr = (int *)pBaseAddr;
    pBaseAddr += 4;

    if (*magic_ptr != PCM_MAGIC) {
        *magic_ptr = PCM_MAGIC;
        p_clear();
    }
    
    printf("pBaseAddr = %p\n", pBaseAddr);

    return 0;
}

void *p_get_base()
{
	return pBaseAddr;
}

int p_clear() {
    if (pBaseAddr == NULL) {
        printf("error: call p_init first\n");
        return -1;
    }

    memset(pBaseAddr, 0, SHM_SIZE/9);

    return 0;
}

void* p_malloc(int size) {
    if (size < 0) {
        printf("error: p_malloc, size must be greater than 0");
        return NULL;
    }

    if (pBaseAddr == NULL) {
        printf("error: call p_init first\n");
        return NULL;
    }

    // save size info
    size += 4;

    char curChar;
    unsigned char mask;
    
    enum {
        STOP = 0,
        LOOKING
    } state;

    state = STOP;
    int iStartBit = 0;
    int *ptrSize = NULL;

    int n;
    for (n=0; n<iBitsCount; n++) {
        mask = 1 << (7 - n%8);
        if (!(pBaseAddr[n/8] & mask)) {
            // nth bit is empty

            switch (state) {
                case STOP:
                    iStartBit = n;
                    state = LOOKING;
                case LOOKING:
                    if (n - iStartBit + 1 >= size) {
                        // we find it 
                        //printf("we find it, ready to set bit\n"); 
                        set_bit_to_one(iStartBit, n);
                        ptrSize = (int *)(pBaseAddr + SHM_SIZE/9 + iStartBit);
                        *ptrSize = size;

                        return (void *)(pBaseAddr + SHM_SIZE/9 + iStartBit + 4); 
                    }

                    break;
                default:
                    break;
            }
        } else {
            // nth bit is not empty
            switch (state) {
                case LOOKING:
                    state = STOP;
                    break;
                default:
                    break;
            }
        }
    }

    return NULL;
}

void set_bit_to_one(int iStartBit, int iEnd) {
    unsigned char mask;
    //printf("in set_bit_to_one, %d, %d", iStartBit, iEnd);
    int n;
    for (n=iStartBit; n<=iEnd; n++) {
        mask = 1 << (7 - n%8);
        pBaseAddr[n/8] |= mask;

        //printf("mask=%d,after set: %d\n", mask,pBaseAddr[n/8]&mask);
    }
}

int p_free(void *addr) {
    if (!addr) {
        printf("invalid arguments\n");
        return -1;
    }

    if (addr < pBaseAddr + SHM_SIZE/9 || addr > pBaseAddr + SHM_SIZE - 1) {
        printf("addr out of range\n"); 
        return -1;
    }
    
    int *ptrSize = (int *)(addr - 4);
    int size = *ptrSize;
    addr = addr - 4;

    int nth = (char*)addr - pBaseAddr - SHM_SIZE/9;
    unsigned char mask;
    int n;
    for (n=nth ; n<nth+size; n++) {
        mask = 1 << (7 - n%8);
        pBaseAddr[n/8] &= ~mask;
    }

    return 0;
}

void *p_new(int pId, int iSize) {
    /*
    if (iSize < 4096) {
        return NULL;
    }
    */

    int iRet = 0;

    iRet = p_search_big_region_node(pId);
    printf("return from p_search_big_region_node: %d\n", iRet);
    if (iRet) {
        printf("id %d already exist\n", pId);
        //return NULL;
    }

    iRet = p_alloc_and_insert(pId, iSize);
    printf("return from p_alloc_and_insert: %d\n", (int)iRet);
    if (iRet != 0) {
        printf("error: p_alloc_and_insert\n");
        //return NULL;
    }

    void *pAddr = p_mmap(NULL, iSize, PROT_READ | PROT_WRITE, pId);
    if (!pAddr) {
        printf("p_mmap return NULL\n");
    }

    return pAddr;
}

int p_delete(int pId) {
    /*
    p_unmap(pId);
    p_tab_delete(pId);
    */
    return p_delete_big_region_node(pId);
}

void *p_get(int pId, int iSize) {
    int iRet = 0;
    
    iRet = p_search_big_region_node(pId);
    if (!iRet) {
        printf("cannot find %d in big region\n", pId);
        return NULL;
    }

    void *pAddr = p_mmap(NULL, iSize, PROT_READ | PROT_WRITE, pId);
    if (!pAddr) {
        printf("p_mmap return NULL\n");
    }

    return pAddr;
}

int p_bind(int id, void *ptr, int size) {
    int offset = (int)(ptr - (void *)pBaseAddr);
    if (offset < 0 || size < 0) {
        return -1;
    }

    return p_bind_(id, offset, size, HPID);
}

void *p_get_bind_node(int pId, int *psize) {
    int iRet = 0;
    int offset;
    
    iRet = p_search_small_region_node(pId, &offset, psize);
    if (iRet < 0) {
        printf("p_search_small_region_node error\n");
        return NULL;
    }

    return (void *)pBaseAddr + offset;
}
