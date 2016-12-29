#ifndef _P_MMAP_H
#define _P_MMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <linux/unistd.h>

#define __NR_p_mmap 322
#define __NR_p_search_big_region_node 323
#define __NR_p_alloc_and_insert 324
#define __NR_p_get_small_region 325
#define __NR_p_bind             326
#define __NR_p_search_small_region_node 327
#define __NR_p_delete_big_region_node 328

#define PCM_MAGIC 0x5A5AA5A5

struct tagMemoryBlock {
    void    *pStart;
    size_t  iLen;
    struct  tagMemoryBlock *next;
};

typedef struct tagMemoryBlock memoryBlock;

struct tagBuddy {
    memoryBlock *pstMemBlock;
};

typedef struct tagBuddy Buddy;

int p_init(int size);

/*
* 清除pcm上方的元数据
*/
int p_clear();

void *p_malloc(int size);

int p_free(void *addr);

void *p_new(int pId, int size);

int p_delete(int pId);

void *p_get(int pId, int iSize);

int p_bind(int id, void *ptr, int size);

void *p_get_bind_node(int pId, int *psize);

void *p_get_base();
/*** helper functions ***/

void set_bit_to_one(int iStartBit, int iEnd);

#endif

