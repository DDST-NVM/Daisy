/* TODO mutex & lock */

#ifndef _SCM_H
#define _SCM_H

#include <linux/rbtree.h>
#include <linux/list.h>

/**
 * magic number
 * ptable addr
 * heaptable addr
 * data
*/

// scm pfn numbers
//#define SCM_PFN_NUM 262144UL
// scm pfn numbers
#define SCM_PFN_NUM 419430UL
//scm pages for big region
#define SCM_BIGREGION_NUM 262144UL
//scm pages for bitmap for big region
#define SCM_BITMAP_NUM 64UL
// scm ptable pfn numbers
#define SCM_PTABLE_PFN_NUM 1024
// scm magic number
#define SCM_MAGIC 0x01234567

/* flags in struct node */
/* TODO use bit such as 0001 0002 0004 0008 0010 */
#define BIG_MEM_REGION 0
#define SMALL_MEM_REGION 1
#define HEAP_REGION 2

struct ptable_node;
struct hptable_node;

struct scm_head {
	unsigned long magic; /* magic number */
	struct rb_root ptable_rb; /* ptale rbtree root */
	struct rb_root hptable_rb; /* hptable rbtree root */
	unsigned long total_size; /* the total memory length (bytes) */
	unsigned long len; /* item number */
	char data[0];
};

/**
 * SCM persist table node
 *
 * FLAGS:
 * 0. big memory region
 * 1. small memory region
 * 2. heap region
 *
 * Requirement:
 * sizeof(hptable_node) === sizeof(ptable_node)
 *
 * Note:
 * _id of 1 & 2 must unique!
 * */
struct ptable_node {
	u64 _id;
	union {
		u64 phys_addr;
		u64 offset;
	};
	u64 size;
	unsigned long flags; /* BIG_MEM_REGION or SMALL_MEM_REGION */
	u64 hptable_id; /* 0 or real _id */
	struct rb_node	ptable_rb;
};

struct hptable_node {
	u64 _id;
	u64 phys_addr;
	u64 size;
	unsigned long flags; /* HEAP_REGION */
	u64 dummy;
	struct rb_node	hptable_rb;
};

/* ptable/hptable node freelist struct */
struct table_freelist {
	void *node_addr;
	struct list_head list;
};

/* linux/mm/scm.c */

/**
 * scm persist table boot step
 * reference to: numa_alloc_distance & numa_reset_distance
 */
void scm_ptable_boot(void);
/**
 * Just traverse the tree to init the freelist in DRAM
 * memblock reserve at the same time
 */
void scm_freelist_boot(void);
/* wapper function: search a node in ptable (big region) 
 * arguments: 
 *   ptable id
 * return NULL if not found 
 */
struct ptable_node *search_big_region_node(u64 _id);
/* wapper function: search a node in ptable (small region) 
 * arguments: 
 *   ptable id
 * return NULL if not found 
 */
struct ptable_node *search_small_region_node(u64 _id);
/* wapper function: search a node in hptable 
 * arguments:
 *   hptable id
 * return NULL if not found 
 */
struct hptable_node *search_heap_region_node(u64 _id);
/* 
 * wapper function: insert a node to ptable rbtree (big region) 
 * arguments: 
 *   ptable id, 
 *   physical address, 
 *   memory size
 * return -1 if error & 0 if success 
 */
int insert_big_region_node(u64 _id, u64 phys_addr, u64 size);
/* wapper function: insert a node to ptable rbtree (small region)
 * arguments: 
 *   ptable id, 
 *   memory offset, 
 *   memory size,
 *   hptable id
 * return -1 if error & 0 if success 
 */
int insert_small_region_node(u64 _id, u64 offset, u64 size, u64 hptable_id);
/* wapper function: insert a node to hptable rbtree 
 * arguments: 
 *   hptable id, 
 *   physical address, 
 *   memory size
 * return -1 if error & 0 if success 
 */
int insert_heap_region_node(u64 _id, u64 phys_addr, u64 size);
/* wapper function: delete a node from ptable (big region) 
 * arguments:
 *   ptable id
 * -1 error, 0 success 
 */
int delete_big_region_node(u64 _id);
/* wapper function: delete a node from ptable (small region) 
 * arguments:
 *   ptable id
 * -1 error, 0 success 
 */
int delete_small_region_node(u64 _id);
/* wapper function: delete a node from hptable
 * arguments:
 *   hptable id
 * -1 error, 0 success 
 */
int delete_heap_region_node(u64 _id);

#endif /* _SCM_H */
