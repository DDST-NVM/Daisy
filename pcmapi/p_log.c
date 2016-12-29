/*
  Persistent Object Store

  Author: Taeho Hwang (htaeh@hanynag.ac.kr)
  Embedded Software Systems Laboratory, Hanyang University
*/

#include <stdio.h>
#include <string.h>
#include <p_log.h>

#include <unistd.h>
#include <syscall.h>

#include <linux/scm.h>

// same as LOG_SIZE_KB and LOG_SIZE in scm.h
/*
#define LOG_SIZE_KB			64		// Unit of KByte (Must be multiple of 16)
#define LOG_SIZE				(LOG_SIZE_KB*1024)
/*
#define KV_SIZE			(16*1024)
#define KV_BASE			(2)
*/
//#define MALLOC_BASE		(KV_SIZE/sizeof(unsigned long))

#define MALLOC_BASE		(4)
#define MALLOC_END		(LOG_SIZE/sizeof(unsigned long)-2)


#define LOG_CNT_ON		0
int clflush_cnt = 0;
int log_cnt = 0;
void print_log_cnt(void)
{
	printf("p_log: clflush_cnt = %d\n", clflush_cnt);
	printf("p_log: log_cnt = %d\n", log_cnt);
}
void clear_log_cnt(void)
{
	clflush_cnt = 0;
	log_cnt = 0;
}



/* HEAPO Consistency Log structure
-----------------------------------------
< [0] STARTorEND		/ [1] INSERTorREMOVE >
-----------------------------------------
< [2] CNT_PTR		/ [3] NULL	>			// KV
< [4] ADDRESS		/ [5] VALUE	>
< [6] ADDRESS		/ [7] VALUE 	>
	... // ptr log increase below
	...
-----------------------------------------
< [2048] CNT_PTR		/ [2049] CNT_FREE>		// POS_MALLOC
< [2050] ADDRESS	/ [2051] VALUE	>
< [2052] ADDRESS	/ [2053] VALUE	>
	... // ptr log increase below
	...
	... // free(seg_free) log increase up
< [8188] ADDRESS	/ [8189] SIZE		>
< [8190] ADDRESS	/ [8191] SIZE		>
-----------------------------------------
// cnt: count
*/

/* Daisy Consistency Log structure
-----------------------------------------
< [0] STARTorEND		/ [1] INSERTorREMOVE >
< [2] pid				/ [3] NULL >
-----------------------------------------
< [4] CNT_PTR		/ [5] CNT_FREE>		// POS_MALLOC
< [6] ADDRESS	/ [7] VALUE	>
< [8] ADDRESS	/ [9] VALUE	>
	... // ptr log increase below
	...
	... // free(seg_free) log increase up
< [8188] ADDRESS	/ [8189] SIZE		>
< [8190] ADDRESS	/ [8191] SIZE		>
-----------------------------------------
// cnt: count
*/

int pnode_log_create(u64 pid)
{
	// TODO: may not need to check...(depend on the logic...)
	int i,j;
    //struct page* page;
    for (i=0; i<SCM_LOG_COUNT; i++) {
        if (scm_log_labels[i] == 0) {
			// TODO: check whether the following setvalue need any guarantee.
            scm_log_labels[i] = 1;
            //break;
			return i;
        }
    }
	
	// log_addr is set when creating ptable_node;
	//return log_addr =  __pa((void *)scm_head + (SCM_PTABLE_PFN_NUM+SCM_BITMAP_LOG_NUM) * PAGE_SIZE + i*LOG_SIZE);
    
	/*
	HEAPO: Call sys_pos_create() to create log object storage.
	name_entry->log_addr = (void *)syscall(299, log_name, LOG_SIZE_KB);
	*/
	// DDST: allocate log storage, kmalloc?? or get_free_page?
	// TODO: define a structure: pnode_log in p_log.h ? (As log should be created before inserting pid into rbtree)
	//pid->log_addr = (void *) kmalloc(LOG_SIZE, GFP_KERNEL);
}

int pnode_log_delete(u64 pid)
{
	struct ptable_node *pnode = null;
	if (pid->flags == BIG_MEM_REGION){
		pnode = search_big_region_node(u64 pid);
	}
	if (pid->flags == SMALL_MEM_REGION){
		pnode = search_small_region_node(u64 pid);
	}
	if (pnode == NULL){
		daisy_printk("error: can not find pnode for id = %d\n",pid);
		return 0;
	}
	// free pid->log_addr. 
	// TODO: free it depending on how to define the data structure
	kfree(pid->log_addr);
	
	// set the corresponding label for log_addr to 0;
	scm_log_labels[pid->_log_id] = 0;
}

/*
int pos_log_map(char *name)

int pos_log_unmap(char *name)
*/

int pnode_log_clear_ptr(unsigned long *log_addr)
{
	/*
	log_addr[KV_BASE] = 0;
	pos_clflush_cache_range(&log_addr[KV_BASE], sizeof(unsigned long));
	*/

	log_addr[MALLOC_BASE] = 0;
	p_clflush_cache_range(&log_addr[MALLOC_BASE], sizeof(unsigned long));
	return 1;
}

int pnode_log_clear_free(unsigned long *log_addr)
{
	log_addr[MALLOC_BASE+1] = 0;
	p_clflush_cache_range(&log_addr[MALLOC_BASE+1], sizeof(unsigned long));

	return 1;
}

int p_transaction_start(u64 pid, unsigned long type)
{
	// check ptable_node
	struct ptable_node *pnode = null;
	if (pid->flags == BIG_MEM_REGION){
		pnode = search_big_region_node(u64 pid);
	}
	if (pid->flags == SMALL_MEM_REGION){
		pnode = search_small_region_node(u64 pid);
	}
	if (pnode == NULL){
		daisy_printk("error: can not find pnode for id = %d\n",pid);
		return 0;
	}

	unsigned long *log_addr;
	log_addr = pnode->log_addr;

	// 1. Clear counter
	pnode_log_clear_ptr(log_addr);
	pnode_log_clear_free(log_addr);

	// 2. Make the transaction flag indicate the START.
	log_addr[0] = PNODE_TS_START;
	p_clflush_cache_range(&log_addr[0], sizeof(unsigned long));

	// 3. Set insert/remove flag
	log_addr[1] = type;
	p_clflush_cache_range(&log_addr[1], sizeof(unsigned long));
	
	return 1;
}

int p_transaction_end(u64 pid)
{
	// check ptable_node
	struct ptable_node *pnode = null;
	if (pid->flags == BIG_MEM_REGION){
		pnode = search_big_region_node(u64 pid);
	}
	if (pid->flags == SMALL_MEM_REGION){
		pnode = search_small_region_node(u64 pid);
	}
	if (pnode == NULL){
		daisy_printk("error: can not find pnode for id = %d\n",pid);
		return 0;
	}

	unsigned long *log_addr;
	log_addr = pnode->log_addr;

	unsigned long type;
	unsigned long addr, value;
	unsigned long count;
	int index, i;

	log_addr = pnode->log_addr;

	// 1. Make the transaction flag indicate the END.
	log_addr[0] = PNODE_TS_END;
	p_clflush_cache_range(&log_addr[0], sizeof(unsigned long));

	// 2. In the case of removal, execute delayed pos_seg_free.
	type = log_addr[1];
	if (type == PNODE_TS_REMOVE) {
		count = log_addr[MALLOC_BASE+1]; // Here, we dont' need to use pos_log_count function.
		index = MALLOC_END;
		
		for (i=0; i<count; i++) {
			addr = log_addr[index];
			value = log_addr[index+1];
			
			//TODO: p_delete / p_unmap
			if (pid->flag == BIG_MEM_REGION){
				p_delete(pid);
			}
			if (pid->flag == SMALL_MEM_REGION){
				temp_addr = pnode->offset;
				// temp_addr - 4 is address of the size of this small region;
				// TODO: modify p_free(void *addr, unsigned long size) to check whether size is right.
				p_free(temp_addr,value);
			}
			//p_seg_free(name, (void *)addr, value);

			index -= 2;
		}
	}

	// 3. Clear counter
	pnode_log_clear_ptr(log_addr);
	pnode_log_clear_free(log_addr);

	// 4. Clear insert/remove flag
	log_addr[1] = 0; // type clear
	p_clflush_cache_range(&log_addr[1], sizeof(unsigned long));
	
	return 1;
}

/*
int pnode_log_insert_kv_ptr(u64 pid, unsigned long addr, unsigned long value)
{
	// check ptable_node
	struct ptable_node *pnode = null;
	if (pid->flags == BIG_MEM_REGION){
		pnode = search_big_region_node(u64 pid);
	}
	if (pid->flags == SMALL_MEM_REGION){
		pnode = search_small_region_node(u64 pid);
	}
	if (pnode == NULL){
		daisy_printk("error: can not find pnode for id = %d",pid);
		return 0;
	}

	unsigned long *log_addr;
	log_addr = pnode->log_addr;

	unsigned long count;
	int index;
	count = log_addr[KV_BASE];
	index = KV_BASE + count*2 +2;

	// 1. Insert log record.
	log_addr[index] = addr;
	log_addr[index+1] = value;
	p_clflush_cache_range(&log_addr[index], 2*sizeof(unsigned long));

	// If system failure occurs before code reaches here,
	// counter value isn't updated.
	// Thus, this log is ignored.
	
	// Also, actual value is not updated while log is being updated
	// because  actual value is updated after pos_log_insert() returns.

	// 2. Increase log counter.
	log_addr[KV_BASE] = count+1;
	p_clflush_cache_range(&log_addr[KV_BASE], sizeof(unsigned long));

#if LOG_CNT_ON == 1
	log_cnt++;
#endif
}
*/

int pnode_log_insert_malloc_ptr(u64 pid, unsigned long addr, unsigned long value)
{
	// check ptable_node
	struct ptable_node *pnode = null;
	if (pid->flags == BIG_MEM_REGION){
		pnode = search_big_region_node(u64 pid);
	}
	if (pid->flags == SMALL_MEM_REGION){
		pnode = search_small_region_node(u64 pid);
	}
	if (pnode == NULL){
		daisy_printk("error: can not find pnode for id = %d\n",pid);
		return 0;
	}

	unsigned long *log_addr;
	log_addr = pnode->log_addr;

	count = log_addr[MALLOC_BASE];
	index = MALLOC_BASE + count*2 +2;

	// 1. Insert log record.
	log_addr[index] = addr;
	log_addr[index+1] = value;
	p_clflush_cache_range(&log_addr[index], 2*sizeof(unsigned long));

	// If system failure occurs before code reaches here,
	// counter value isn't updated.
	// Thus, this log is ignored.
	
	// Also, actual value is not updated while log is being updated
	// because  actual value is updated after pnode_log_insert() returns.

	// 2. Increase log counter.
	log_addr[MALLOC_BASE] = count+1;
	p_clflush_cache_range(&log_addr[MALLOC_BASE], sizeof(unsigned long));
}

int pnode_log_insert_malloc_free(u64 pid, unsigned long addr, unsigned long value)
{
	// check ptable_node
	struct ptable_node *pnode = null;
	if (pid->flags == BIG_MEM_REGION){
		pnode = search_big_region_node(u64 pid);
	}
	if (pid->flags == SMALL_MEM_REGION){
		pnode = search_small_region_node(u64 pid);
	}
	if (pnode == NULL){
		daisy_printk("error: can not find pnode for id = %d\n",pid);
		return 0;
	}

	unsigned long *log_addr;
	log_addr = pnode->log_addr;

	count = log_addr[MALLOC_BASE+1];
	index = MALLOC_END - count*2;

	// 1. Insert log record.
	log_addr[index] = addr;
	log_addr[index+1] = value;
	p_clflush_cache_range(&log_addr[index], 2*sizeof(unsigned long));

	// If system failure occurs before code reaches here,
	// counter value isn't updated.
	// Thus, this log is ignored.
	
	// Also, actual value is not updated while log is being updated
	// because  actual value is updated after pnode_log_insert() returns.

	// 2. Increase log counter.
	log_addr[MALLOC_BASE+1] = count+1;
	p_clflush_cache_range(&log_addr[MALLOC_BASE+1], sizeof(unsigned long));
}


// #define p_write_value(pid, addr, value)	
/*
int p_write_value_kv(u64 pid, unsigned long *addr, unsigned long value)
{
	pnode_log_insert_kv_ptr(pid, (unsigned long)addr, *addr);
	*addr = value;
	p_clflush_cache_range((void *)addr, sizeof(unsigned long));
	return 1;
}

int p_write_value_kv_noflush(u64 pid, unsigned long *addr, unsigned long value)
{
	pnode_log_insert_kv_ptr(pid, (unsigned long)addr, *addr); //record the old value (undo log)
	*addr = value;
	return 1;
}
*/
int p_write_value_malloc(u64 pid, unsigned long *addr, unsigned long value)
{
	pnode_log_insert_malloc_ptr(pid, (unsigned long)addr, *addr);
	*addr = value;
	p_clflush_cache_range((void *)addr, sizeof(unsigned long));
	return 1;
}

int p_write_value_free(u64 pid, unsigned long *addr, unsigned long value)
{
	pnode_log_insert_malloc_free(pid, (unsigned long)addr, *addr);
	*addr = value;
	p_clflush_cache_range((void *)addr, sizeof(unsigned long));
	return 1;
}

int p_recovery_ptr(u64 pid, unsigned long *log_addr)
{
	unsigned long count;
	unsigned long *addr;
	int index, i;


	// 1. KV
	/*
	count = log_addr[KV_BASE];
	index = KV_BASE + count*2;

	// Reverse order
	for (i=0; i<count; i++) {
		addr = (unsigned long *)log_addr[index];	// target address
		*addr = log_addr[index+1];			// target value
		pos_clflush_cache_range(addr, sizeof(unsigned long));

		index -= 2;
	}
	*/

	// 2. MALLOC
	count = log_addr[MALLOC_BASE];
	index = MALLOC_BASE + count*2;

	// Reverse order
	for (i=0; i<count; i++) {
		addr = (unsigned long *)log_addr[index];	// target address
		*addr = log_addr[index+1];			// target value
		p_clflush_cache_range(addr, sizeof(unsigned long));

		index -= 2;
	}

	return 1;
}

int p_recovery_free(u64 pid, unsigned long *log_addr)
{
	unsigned long count;
	unsigned long addr, value;
	int index, i;

	count = log_addr[MALLOC_BASE+1];
	index = MALLOC_END - count*2;
	
	// Reverse order
	for (i=0; i<count; i++) {
		addr = log_addr[index];
		value = log_addr[index+1];
			
		//TODO: p_delete / p_unmap
		if (pid->flag == BIG_MEM_REGION){
			p_delete(pid);
		}
		if (pid->flag == SMALL_MEM_REGION){
			struct ptable_node *pnode = search_small_region_node(u64 pid);
			temp_addr = pnode->offset;
			// temp_addr - 4 is address of the size of this small region;
			// TODO: modify p_free(void *addr, unsigned long size) to check whether size is right.
			p_free(temp_addr,value);
		}
		//pos_seg_free(name, (void *)addr, value);
		
		index += 2;
	}

	return 1;
}

int p_recovery_insert(u64 pid, unsigned long *log_addr)
{
	int se_type; // START or END
	se_type = log_addr[0];

	if (se_type == PNODE_TS_END) {
		// Transaction ended normally.
		// So, we can do nothing.
		return 1;
	} else if (se_type == PNODE_TS_START) {
		// Transaction ended abnormally.
		// Execute undo both PTR and FREE.
		p_recovery_ptr(pid, log_addr);
		pnode_log_clear_ptr(log_addr);
		
		p_recovery_free(pid, log_addr);
		pnode_log_clear_free(log_addr);
	}
	return 1;
}

int p_recovery_remove(u64 pid, unsigned long *log_addr)
{
	int se_type; // START or END

	se_type = log_addr[0];

	if (se_type == PNODE_TS_START) {
		// Transaction ended abnormally.
		// Execute undo PTR.
		p_recovery_ptr(pid, log_addr);
		pnode_log_clear_ptr(log_addr);
	} else if (se_type == PNODE_TS_END) {
		// Transaction ended normally.
		// But, pos_seg_free may not be excuted normally.
		// So. execute redo FREE.
		p_recovery_ptr(pid, log_addr);
		pnode_log_clear_ptr(log_addr);
	}
	
	return 1;
}

int p_recovery(u64 pid)
{
	// check ptable_node
	struct ptable_node *pnode = null;
	if (pid->flags == BIG_MEM_REGION){
		pnode = search_big_region_node(u64 pid);
	}
	if (pid->flags == SMALL_MEM_REGION){
		pnode = search_small_region_node(u64 pid);
	}
	if (pnode == NULL){
		daisy_printk("error: can not find pnode for id = %d\n",pid);
		return 0;
	}

	unsigned long *log_addr;
	log_addr = pnode->log_addr;
	unsigned long type;
	type = log_addr[1];

	switch (type) {
	case PNODE_TS_INSERT:
		return p_recovery_insert(pid, log_addr);
	case PNODE_TS_REMOVE:
		return p_recovery_remove(pid, log_addr);								
	default:
		// Transaction was about to start or ended normally.
		// But, power failure occured while type is being updated.
		// So, in this case, we can do nothing.
		log_addr[1] = 0;
		p_clflush_cache_range(&log_addr[1], sizeof(unsigned long));
		return 1;
	}
}

#define mb() 	asm volatile("mfence":::"memory")

static inline void clflush(volatile void *__p)
{
	//asm volatile("clflush %0" : "+m" (*(volatile char __force *)__p));
	asm volatile("clflush %0" : "+m" (*(volatile char *)__p));

#if LOG_CNT_ON == 1
	clflush_cnt++;
#endif
}

int x86_64_clflush_size = 64;

/**
 * clflush_cache_range - flush a cache range with clflush
 * @addr:	virtual start address
 * @size:	number of bytes to flush
 *
 * clflush is an unordered instruction which needs fencing with mfence
 * to avoid ordering issues.
 */
void p_clflush_cache_range(void *vaddr, unsigned int size)
{
//	return ; // for test...

	void *vend = vaddr + size - 1;

	mb();

	for (; vaddr < vend; vaddr += x86_64_clflush_size)
		clflush(vaddr);
	/*
	 * Flush any possible final partial cacheline:
	 */
	clflush(vend);

	mb();
}
