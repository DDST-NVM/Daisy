#ifndef _P_LOG_H
#define _P_LOG_H

//#include <linux/list.h>

#define PNODE_TS_START	0x76934751
#define PNODE_TS_END	0xA1450D30

#define PNODE_TS_INSERT	0x12345678
#define PNODE_TS_REMOVE	0x87654321

// TODO: should define pnode_log or not...
/*
struct pnode_log {
	void *log_addr;
	struct list_head list;
};
*/
extern int pnode_log_create(u64 pid);
extern int pnode_log_delete(u64 pid);

extern int p_transaction_start(u64 pid, unsigned long type);
extern int p_transaction_end(u64 pid);
extern int pnode_log_insert_malloc_ptr(u64 pid, unsigned long addr, unsigned long value);
extern int pnode_log_insert_malloc_free(u64 pid, unsigned long addr, unsigned long value);
/*
extern int p_write_value_kv(u64 pid, unsigned long *addr, unsigned long value);
extern int p_write_value_kv_noflush(u64 pid, unsigned long *addr, unsigned long value);
*/
extern int p_write_value_malloc(u64 pid, unsigned long *addr, unsigned long value);
extern int p_write_value_free(u64 pid, unsigned long *addr, unsigned long value);
extern int p_recovery(u64 pid);
extern void p_clflush_cache_range(void *vaddr, unsigned int size);

extern void print_log_cnt(void);
extern void clear_log_cnt(void);
