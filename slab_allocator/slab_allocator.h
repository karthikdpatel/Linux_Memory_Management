#define SCULL_QSET 2
#define SCULL_QUANTUM 5


struct mm_cache {
	size_t object_size;
	size_t num_blocks;
	size_t num_free_blocks;
	void * start_addr;
	
	struct mm_slab_block * free_list;
	struct mm_slab_block * allocated_list;
	struct mm_cache * next_mm_cache;
	
	spinlock_t mm_cache_spinlock;
	unsigned long spinlock_irq_flag;
};

struct mm_slab_block {
	void * start_addr;
	size_t obj_size;
	size_t slab_num;
	struct mm_slab_block * next_mm_slab_block;
};
