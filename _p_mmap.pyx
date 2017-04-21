cdef extern from "p_mmap.h":
	struct tagMemoryBlock:
		void *pStart
		size_t iLen
		tagMemoryBlock *next
	struct Buddy:
		pass
	int p_init(int size)
	int p_clear()
	void* p_malloc(int size)
	int p_free(void* addr)
	void* p_new(int pId, int size)
	int p_delete(int pId)
	void* p_get(int pId, int iSize)
	int p_bind(int id, void *ptr, int size)
	void* p_get_bind_node(int pId, int *pSize)
	void* p_get_base()
	void set_bit_to_one(int iStartBit, int iEnd)

def p_init_fun(arg):
	p_init(arg)
def p_clear_fun():
	p_clear()
def p_get_base_fun():
	<void*>p_get_base()
def p_malloc_fun(arg):
	<void*>p_malloc(arg)
def p_free_fun(arg):
	p_free(<void*>arg)
def p_new_fun(arg1, arg2):
	<void*>p_new(arg1, arg2)
def p_delete_fun(arg):
	p_delete(arg)
def p_bind_fun(arg1, arg2, arg3):
	p_bind(arg1, <void*>arg2, arg3)
def p_get_bind_node_fun(arg1):
	cdef int arg2
	<void*>p_get_bind_node(arg1,&arg2)
def p_get_fun(arg1, arg2):
	<void*>p_get(arg1, arg2)
def set_bit_to_one_fun(arg1, arg2):
	set_bit_to_one(arg1, arg2)
