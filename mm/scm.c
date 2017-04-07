#include <linux/scm.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/syscalls.h>

/* pointer to scm memory head */
static struct scm_head *scm_head;
/* pointer to ptable node freelist */
static struct table_freelist *table_freelist;

/* code FOR DEBUG */
static u64 freecount = 0; // count freelist

static char *scm_free_pages;

/* print scm freelist status */
static void scm_print_freelist(void)
{
	struct table_freelist *tmp;
	int i=0;
	daisy_printk("freelist %lu: ", freecount);
	list_for_each_entry(tmp, &table_freelist->list, list) {
		daisy_printk("%lu %lu\t", tmp->node_addr, ((unsigned long)tmp->node_addr-(unsigned long)&scm_head->data)/sizeof(struct ptable_node));
		++i;
		if (i>=10) break;
	}
	daisy_printk("\n");
}

/* print scm ptable node infomation */
static void scm_print_pnode(struct ptable_node *n)
{
	if (n) {
		daisy_printk("id: %lu, vaddr %lu\n", n->_id, n);
	} else {
		daisy_printk("NULL\n");
	}
}

/* generate fake scm init data for test */
static void scm_fake_initdata(void)
{
	struct ptable_node *pnode;
	struct hptable_node *hnode;

	if (!scm_head) return;

	pnode = (struct ptable_node *)((char *)&scm_head->data + 3*sizeof(struct ptable_node));
	scm_head->ptable_rb.rb_node = &pnode->ptable_rb;
	hnode = (struct hptable_node *)((char *)&scm_head->data + 5*sizeof(struct ptable_node));
	scm_head->hptable_rb.rb_node = &hnode->hptable_rb;
}

/* test function for scm api */
void scm_full_test(void)
{
	struct ptable_node *n;
	struct page *page;

	insert_big_region_node(345, 0, 0);
	scm_print_freelist();
	insert_small_region_node(344, 0, 0, 557);
	insert_heap_region_node(557, 0, 0);
	insert_big_region_node(342, 0, 0);
	scm_print_freelist();
	n = search_big_region_node(342);
	scm_print_pnode(n);
	n = search_small_region_node(344);
	scm_print_pnode(n);
	n = search_heap_region_node(557);
	scm_print_pnode(n);
	delete_big_region_node(342);
	delete_heap_region_node(557);
	delete_small_region_node(344);
	delete_big_region_node(345);
	scm_print_freelist();
	page = alloc_pages(GFP_KERNEL | GFP_SCM, 0);
	daisy_printk("alloc_pages: %s %lu %lu %lu\n", page_zone(page)->name, page_to_pfn(page), PFN_PHYS(page_to_pfn(page)), page_address(page));
	page = alloc_pages(GFP_KERNEL | GFP_DMA, 0);
	daisy_printk("alloc_pages: %s %lu %lu %lu\n", page_zone(page)->name, page_to_pfn(page), PFN_PHYS(page_to_pfn(page)), page_address(page));
	page = alloc_pages(GFP_KERNEL, 0);
	daisy_printk("alloc_pages: %s %lu %lu %lu\n", page_zone(page)->name, page_to_pfn(page), PFN_PHYS(page_to_pfn(page)), page_address(page));
}
/* end code FOR DEBUG */

/* reserve scm ptable/hptable memory */
static void  reserve_scm_ptable_memory(void)
{
	unsigned long size;
	phys_addr_t phys;
	struct scm_head hd;
	/* get the first 1024 scm pages */
	size = (SCM_PTABLE_PFN_NUM+SCM_BIGREGION_NUM+SCM_BITMAP_NUM) * PAGE_SIZE;
	/* pages in ZONE_SCM */
	phys = PFN_PHYS(max_pfn_mapped)-(SCM_PFN_NUM<<PAGE_SHIFT);
	memblock_reserve(phys, size);
	scm_head = (struct scm_head*)__va(phys);
	
	scm_free_pages = (char *)__va(phys + SCM_PTABLE_PFN_NUM * PAGE_SIZE);

	early_printk("scm_start_phys: %lu scm_head vaddr %lu\n", phys, scm_head);
	early_printk("Get start pfn: %lu， max_pfn: %lu\n", phys >> PAGE_SHIFT, max_pfn_mapped);
	/* record the size */
	/* TODO if scm has old data, total_size cannot change, do a realloc; now just check */
	if (scm_head->magic == SCM_MAGIC && scm_head->total_size !=size) {
		daisy_printk("TODO we need a warning or realloc here.\n");
	}

	scm_head->total_size = SCM_PTABLE_PFN_NUM * PAGE_SIZE;
}

/* Init a total new SCM (no data) */
static void scm_ptable_init(void)
{
	scm_head->magic = SCM_MAGIC;
	scm_head->ptable_rb = RB_ROOT;
	scm_head->hptable_rb = RB_ROOT;
	/* calc scm_head->len */
	scm_head->len = (scm_head->total_size - sizeof(struct scm_head))/sizeof(struct ptable_node);
	/* do i need a whole memset (set to 0)? */
}

/* reserve used scm memory (do not return to buddy system) */
static void __meminit  scm_reserve_used_memory(void) {
	struct rb_node *nd;
	/* ptable */
	if (!RB_EMPTY_ROOT(&scm_head->ptable_rb)) {
		for (nd = rb_first(&scm_head->ptable_rb); nd; nd = rb_next(nd)) {
			struct ptable_node *touch;
			touch = rb_entry(nd, struct ptable_node, ptable_rb);
			/* ignore small memory region */
			if (touch->flags == BIG_MEM_REGION) {
				memblock_reserve(touch->phys_addr, touch->size);
			}
		}
	}
	/* hptable */
	if (!RB_EMPTY_ROOT(&scm_head->hptable_rb)) {
		for (nd = rb_first(&scm_head->hptable_rb); nd; nd = rb_next(nd)) {
			struct hptable_node *touch;
			touch = rb_entry(nd, struct hptable_node, hptable_rb);
			memblock_reserve(touch->phys_addr, touch->size);
		}
	}
}

/**
 * scm persist table boot step
 * reference to: numa_alloc_distance & numa_reset_distance
 */
void  scm_ptable_boot(void)
{
	reserve_scm_ptable_memory();

	/**
	 * check magic number to decide how to init
	 * 1. clear scm, just set scm_head data
	 * 2. traverse tree to create freelist (do it later)
	 * */
	daisy_printk("scm_head: %lu %lu %lu %lu\n",
			scm_head->magic,
			scm_head->ptable_rb,
			scm_head->hptable_rb,
			scm_head->len);
	if (scm_head->magic != SCM_MAGIC) {
		/* this is a new SCM */
		scm_ptable_init();
	int i;
        for (i=0; i<SCM_BIGREGION_NUM;i++) {
           scm_free_pages[i] = 0;
        }
		//scm_fake_initdata();
	} else {
		/* SCM with data! */
		scm_reserve_used_memory();
	}
}

/**
 * Just traverse the tree to init the freelist in DRAM
 * memblock reserve at the same time
 */
void __meminit scm_freelist_boot(void)
{
	struct table_freelist *tmp;
	unsigned long index;
	struct rb_node *nd;
	char *usage_map = (char *)kmalloc(scm_head->len, GFP_KERNEL);

	table_freelist = (struct table_freelist *) kmalloc(sizeof(struct table_freelist), GFP_KERNEL);
	INIT_LIST_HEAD(&table_freelist->list);

	for (index = 0; index < scm_head->len; ++index) {
		usage_map[index] = 0;
	}
	/* ptable */
	if (!RB_EMPTY_ROOT(&scm_head->ptable_rb)) {
		for (nd = rb_first(&scm_head->ptable_rb); nd; nd = rb_next(nd)) {
			struct ptable_node *touch;
			touch = rb_entry(nd, struct ptable_node, ptable_rb);
			/* ignore small memory region */
			if (touch->flags == BIG_MEM_REGION) {
				index = ((unsigned long) touch - (unsigned long) &scm_head->data) / sizeof(struct ptable_node);
				usage_map[index] = 1;
			}
		}
	}
	/* hptable */
	if (!RB_EMPTY_ROOT(&scm_head->hptable_rb)) {
		for (nd = rb_first(&scm_head->hptable_rb); nd; nd = rb_next(nd)) {
			struct hptable_node *touch;
			touch = rb_entry(nd, struct hptable_node, hptable_rb);
			index = ((unsigned long) touch - (unsigned long) &scm_head->data) / sizeof(struct hptable_node);
			usage_map[index] = 1;
		}
	}
	/* freelist */
	for (index = 0; index < scm_head->len; ++index) {
		if (usage_map[index] == 0) {
			tmp = (struct table_freelist *) kmalloc(sizeof(struct table_freelist), GFP_KERNEL);
			tmp->node_addr = (char *) &scm_head->data + index * sizeof(struct ptable_node);
			list_add_tail(&tmp->list, &table_freelist->list);
			freecount++;
		}
	}
	/* TODO test SCM is not new */
	scm_print_freelist();
//	scm_full_test();
	kfree(usage_map);
}

/* pop an item from freelist, free the item, return the addr (NULL if no more) */
static void *get_freenode_addr(void)
{
	void *ret;
	struct table_freelist *entry;
	if (list_empty(&table_freelist->list)) {
		return NULL;
	}
	entry = list_first_entry(&table_freelist->list, struct table_freelist, list);
	ret = entry->node_addr;
	list_del(&entry->list);
	freecount--;
	kfree(entry);
	return ret;
}

/* 
 * insert new node to ptable rbtree 
 * arguments: 
 *   ptable id, 
 *   physical address, 
 *   memory size, 
 *   hptable id, 
 *   flags
 * return -1 if error & 0 if success 
 */
static int insert_ptable_node_rb(u64 _id, u64 phys_addr, u64 size, u64 hptable_id, unsigned long flags)
{
	struct rb_node **n = &scm_head->ptable_rb.rb_node;
	struct rb_node *parent = NULL;
	struct ptable_node *new, *touch;
	new = (struct ptable_node *)get_freenode_addr();
	if (!new) {
		return -1;
	}
	new->_id = _id;
	new->phys_addr = phys_addr;
	new->size = size;
	new->flags = flags;
	new->hptable_id = hptable_id;

	/* insert to rbtree */
	while (*n) {
		parent = *n;
		touch = rb_entry(parent, struct ptable_node, ptable_rb);
		if (_id < touch->_id) {
			n = &(*n)->rb_left;
		} else if (_id > touch->_id) {
			n = &(*n)->rb_right;
		} else {
			return -1;
		}
	}
	rb_link_node(&new->ptable_rb, parent, n);
	rb_insert_color(&new->ptable_rb, &scm_head->ptable_rb);
	return 0;
}

/* 
 * insert new node to hptable rbtree 
 * arguments: 
 *   hptable id, 
 *   physical address, 
 *   memory size, 
 * return -1 if error & 0 if success 
 */
static int insert_hptable_node_rb(u64 _id, u64 phys_addr, u64 size)
{
	struct rb_node **n = &scm_head->hptable_rb.rb_node;
	struct rb_node *parent = NULL;
	struct hptable_node *new, *touch;
	new = (struct hptable_node *)get_freenode_addr();
	if (!new) {
		return -1;
	}
	new->_id = _id;
	new->phys_addr = phys_addr;
	new->size = size;
	new->flags = HEAP_REGION;
	/* insert to rbtree */
	while (*n) {
		parent = *n;
		touch = rb_entry(parent, struct hptable_node, hptable_rb);
		if (_id < touch->_id) {
			n = &(*n)->rb_left;
		} else if (_id > touch->_id) {
			n = &(*n)->rb_right;
		} else {
			return -1;
		}
	}
	rb_link_node(&new->hptable_rb, parent, n);
	rb_insert_color(&new->hptable_rb, &scm_head->hptable_rb);
	return 0;
}

/* 
 * wapper function: insert a node to ptable rbtree (big region) 
 * arguments: 
 *   ptable id, 
 *   physical address, 
 *   memory size
 * return -1 if error & 0 if success 
 */
int insert_big_region_node(u64 _id, u64 phys_addr, u64 size)
{
	return insert_ptable_node_rb(_id, phys_addr, size, 0, BIG_MEM_REGION);
}

/* wapper function: insert a node to ptable rbtree (small region)
 * arguments: 
 *   ptable id, 
 *   memory offset, 
 *   memory size,
 *   hptable id
 * return -1 if error & 0 if success 
 */
int insert_small_region_node(u64 _id, u64 offset, u64 size, u64 hptable_id)
{
	return insert_ptable_node_rb(_id, offset, size, hptable_id, SMALL_MEM_REGION);
}

/* wapper function: insert a node to hptable rbtree 
 * arguments: 
 *   hptable id, 
 *   physical address, 
 *   memory size
 * return -1 if error & 0 if success 
 */
int insert_heap_region_node(u64 _id, u64 phys_addr, u64 size)
{
	return insert_hptable_node_rb(_id, phys_addr, size);
}

/* 
 * search node in ptable rbtree
 * arguments: 
 *   ptable id, 
 *   flags
 * return NULL if not found 
 */
static struct ptable_node *search_ptable_node_rb(u64 _id, unsigned long flags)
{
	struct rb_node *n;
	struct ptable_node *touch;
	if (flags != BIG_MEM_REGION && flags != SMALL_MEM_REGION) {
		return NULL;
	}
	n = scm_head->ptable_rb.rb_node;
	while (n) {
		touch = rb_entry(n, struct ptable_node, ptable_rb);
		if (_id < touch->_id) {
			n = n->rb_left;
		} else if (_id > touch->_id) {
			n = n->rb_right;
		} else {
			/* do a simple check */
			if (touch->flags == flags) {
				return touch;
			} else {
				return NULL;
			}
		}
	}
	return NULL;
}

/* 
 * search node in hptable rbtree
 * arguments: 
 *   hptable id
 * return NULL if not found 
 */
static struct hptable_node *search_hptable_node_rb(u64 _id)
{
	struct rb_node *n;
	struct hptable_node *touch;
	n = scm_head->hptable_rb.rb_node;
	while (n) {
		touch = rb_entry(n, struct hptable_node, hptable_rb);
		if (_id < touch->_id) {
			n = n->rb_left;
		} else if (_id > touch->_id) {
			n = n->rb_right;
		} else {
			/* do a simple check */
			if (touch->flags == HEAP_REGION) {
				return touch;
			} else {
				return NULL;
			}
		}
	}
	return NULL;
}

/* wapper function: search a node in ptable (big region) 
 * arguments: 
 *   ptable id
 * return NULL if not found 
 */
struct ptable_node *search_big_region_node(u64 _id)
{
	return search_ptable_node_rb(_id, BIG_MEM_REGION);
}

/* wapper function: search a node in ptable (small region) 
 * arguments: 
 *   ptable id
 * return NULL if not found 
 */
struct ptable_node *search_small_region_node(u64 _id)
{
	return search_ptable_node_rb(_id, SMALL_MEM_REGION);
}

/* wapper function: search a node in hptable 
 * arguments:
 *   hptable id
 * return NULL if not found 
 */
struct hptable_node *search_heap_region_node(u64 _id)
{
	return search_hptable_node_rb(_id);
}

/* release free node to freelist 
 * arguments:
 *   node address
 */
static void add_freenode_addr(void *addr)
{
	struct table_freelist *tmp;
	tmp= (struct table_freelist *)kmalloc(sizeof(struct table_freelist), GFP_KERNEL);
	tmp->node_addr = addr;
	list_add(&tmp->list, &table_freelist->list);
	freecount++;
}

/* 
 * delete node in ptable rbtree 
 * arguments:
 *   ptable id
 *   flags
 * -1 error, 0 success 
 */
static int delete_ptable_node_rb(u64 _id, unsigned long flags)
{
	struct ptable_node *n;
	n = search_ptable_node_rb(_id, flags);
	if (!n) {
		return -1;
	}
	rb_erase(&n->ptable_rb, &scm_head->ptable_rb);
	add_freenode_addr((void *)n);
	return 0;
}

/* 
 * delete node in hptable rbtree
 * arguments:
 *   hptable id
 * -1 error, 0 success 
 */
static int delete_hptable_node_rb(u64 _id)
{
	struct hptable_node *n;
	n = search_hptable_node_rb(_id);
	if (!n) {
		return -1;
	}
	rb_erase(&n->hptable_rb, &scm_head->hptable_rb);
	add_freenode_addr((void *)n);
	return 0;
}

/* wapper function: delete a node from ptable (big region) 
 * arguments:
 *   ptable id
 * -1 error, 0 success 
 */
int delete_big_region_node(u64 _id)
{
	return delete_ptable_node_rb(_id, BIG_MEM_REGION);
}

/* wapper function: delete a node from ptable (small region) 
 * arguments:
 *   ptable id
 * -1 error, 0 success 
 */
int delete_small_region_node(u64 _id)
{
	return delete_ptable_node_rb(_id, SMALL_MEM_REGION);
}

/* wapper function: delete a node from hptable
 * arguments:
 *   hptable id
 * -1 error, 0 success 
 */
int delete_heap_region_node(u64 _id)
{
	return delete_hptable_node_rb(_id);
}


static void *get_free_page(u64 num) {
    int i,j;
    struct page* page;
    for (i=0; i<SCM_BIGREGION_NUM; i++) {
        if (scm_free_pages[i] == 0) {
            //scm_free_pages[i] = 1;
            break;
        }
    }

    if (i == SCM_BIGREGION_NUM) {
        return NULL;
    } else {
		for (j=i; j<i+num; j++)
		{
			scm_free_pages[i] = 1;
		}

        return __pa((void *)scm_head + (i+SCM_PTABLE_PFN_NUM+SCM_BITMAP_NUM) * PAGE_SIZE);
    }
}

/* syscall functions */


/*
 * delete a node from big region
 * arguments:
 *   ptable id
 * -1 error, 0 success
 */
SYSCALL_DEFINE1(p_delete_big_region_node,u64, _id)
{
	printk("Delete node %d\n",_id);
	return delete_big_region_node(_id);
}


/*
 * search a node from big region
 * arguments:
 *   ptable id
 * exist return true, else return false
 */

SYSCALL_DEFINE1(p_search_big_region_node, unsigned long, id) {
	struct ptable_node *node = search_big_region_node(id);
	return (node != NULL);
}

/*
 * alloc small region and insert node to hptable
 * arguments:
 *   ptable id
 *   memory size
 * return node
 */
SYSCALL_DEFINE2(p_alloc_and_insert, unsigned long, id, unsigned long, size) {
	int iRet = 0;
	struct page *page;
	//allocate for more than 8M
	if(size>=8*1024*1024)
	{
		void *pAddr = get_free_page(size);
		iRet = insert_big_region_node(id, (u64)pAddr, size);
		if (iRet != 0) {
			daisy_printk("error: insert_big_region_node\n");
		}
		return iRet;
	}
    // decide the order in buddy system
    int order = 0;
	int thissize = 4096;
	while(thissize < size) {
		thissize*=2;
	    order++;
	}
	daisy_printk("alloc and insert: id=%d sz=%d\n",id,size);
	printk("Alloc order %d\n",order);
    // alloc page
	page = alloc_pages(GFP_KERNEL | GFP_SCM, order);
	if (page == NULL) {
		daisy_printk("error: alloc_pages\n");
		return -1;
	}

	void *pAddr = (page_to_pfn(page) << PAGE_SHIFT);
	if (pAddr == NULL) {
		daisy_printk("error: page_address");
		return -1;
	} else {
		daisy_printk("page's phys addr = %p\n", pAddr);
	}

    // insert into ptable
	iRet = insert_big_region_node(id, (u64)pAddr, size);
	if (iRet != 0) {
		daisy_printk("error: insert_big_region_node\n");
	}

	return iRet;
}

/*
 * get small region infomation
 * arguments:
 *   hptable id
 *   memory size
 * return node
 */
SYSCALL_DEFINE2(p_get_small_region, unsigned long, id, unsigned long, size) {

	// get id from inode
	struct hptable_node *pHpNode = search_heap_region_node(id);
	int iRet = 0;
	if (pHpNode != NULL) {
		daisy_printk("find heap region per program\n");
		return 0;
	} else {
		daisy_printk("can not find heap region per program\n");
	}
	//allocate for more than 8M
	if(size>=8*1024*1024)
	{
		void *pAddr = get_free_page(size);
		iRet = insert_heap_region_node(id, (u64)pAddr, size);
		if (iRet != 0) {
			daisy_printk("error: insert_big_region_node\n");
			return -1;
		}
		return iRet;
	}
    // decide the order in buddy system
	int order = 0;
	int thissize = 4096;
	while(thissize < size) {
		thissize*=2;
	    order++;
	}
    //alloc page
	struct page *page = alloc_pages(GFP_KERNEL | GFP_SCM, order);
	if (page == NULL) {
		daisy_printk("error: alloc_pages\n");
		return -1;
	}

	void *pAddr = (page_to_pfn(page) << PAGE_SHIFT);
	if (pAddr == NULL) {
		daisy_printk("error: page_address");
		return -1;
	} else {
		daisy_printk("page's phys addr = %p\n", pAddr);
	}
    // insert into hptable
	iRet = insert_heap_region_node(id, (u64)pAddr, size);
	if (iRet != 0) {
		daisy_printk("error: insert_big_region_node\n");
		return -1;
	}

	return iRet;
}

/*
 * bind id to small region node
 * arguments:
 *   ptable id
 *   memory offset
 *   memory size
 *   hptable id
 * return node
 */
SYSCALL_DEFINE4(p_bind, unsigned long, id, unsigned long, offset, unsigned long, size, unsigned long, hptable_id) {
    // check hptable	
    struct hptable_node *pHpNode = search_heap_region_node(hptable_id);
    if (pHpNode == NULL) {
		daisy_printk("can not find heap region per program\n");
        return -1;
	}

    return insert_small_region_node(id, offset, size, hptable_id);
}

/*
 * search a node from small region
 * arguments:
 *   ptable id
 *   memory offset
 *   memory size
 * success return 0 else return -1
 */
SYSCALL_DEFINE3(p_search_small_region_node, unsigned long, id, void *, poffset, void *, psize) {
    int *po = (int *)poffset;
    int *ps = (int *)psize;

    struct ptable_node *pnode = search_small_region_node(id);
    if (pnode == NULL) {
        return -1;
   }
    
    //TODO: hptable_id check
    if (po != NULL) {
        *po = pnode->offset;
    }

    if (ps != NULL) {
        *ps = pnode->size;
    }

    return 0;
}
