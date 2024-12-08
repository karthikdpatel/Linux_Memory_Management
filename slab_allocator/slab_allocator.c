#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "slab_allocator.h"
#include <linux/spinlock.h>

MODULE_LICENSE("Dual BSD/GPL");

static int mm_slab_open(struct inode * inode, struct file * filp);
static int mm_slab_release(struct inode * inode, struct file * filp);

void * start_addr;
struct mm_cache * cache;

static void * allocate_memory_internal(size_t mem_size)
{
	struct mm_cache * mm_cache = cache;
	
	while(mm_cache && mm_cache->object_size != mem_size)
	{
		mm_cache->next_mm_cache;
	}
	
	if(!mm_cache)
	{
		printk(KERN_ERR "SLAB_ALLOCATOR : Requested cache is not available, requested size :%zu\n", mem_size);
		return NULL;
	}
	
	if(mm_cache->num_free_blocks == 0)
	{
		printk(KERN_ERR "SLAB_ALLOCATOR : No free blocks available in cache\n", mem_size);
		return NULL;
	}
}

static void * allocate_memory(size_t mem_size)
{
	if(mem_size > 32)
	{
		printk(KERN_ERR "SLAB_ALLOCATOR : Requested memory is greater than 32\n");
		return NULL;
	}
	
	if(mem_size > 16 && mem_size <= 32)
	{
		return allocate_memory_internal(32);
	}
	else if (mem_size > 8 && mem_size <= 16)
	{
		return allocate_memory_internal(16);
	}
	else
	{
		return allocate_memory_internal(8);
	}
}


static void initialize_slab(struct mm_cache * mm_cache)
{
	size_t i=0;
	void * start_addr = mm_cache->start_addr;
	struct mm_slab_block * slab_block;
	struct mm_slab_block * prev_slab_block = NULL;
	
	slab_block = kmalloc(sizeof(struct mm_slab_block), GFP_KERNEL);
	slab_block->start_addr = start_addr + i*mm_cache->object_size;
	slab_block->obj_size = mm_cache->object_size;
	slab_block->next_mm_slab_block = prev_slab_block;
	prev_slab_block = slab_block;
	
	mm_cache->free_list = slab_block;
	
	for(i = 1; i < mm_cache->num_blocks; i++)
	{
		slab_block = kmalloc(sizeof(struct mm_slab_block), GFP_KERNEL);
		slab_block->start_addr = start_addr + i*mm_cache->object_size;
		slab_block->obj_size = mm_cache->object_size;
		slab_block->next_mm_slab_block = prev_slab_block;
		prev_slab_block = slab_block;
	}
	
	printk("SLAB_ALLOCATOR : Initialized slabs, total=%zu, size of each memory chunk=%zu\n", mm_cache->num_blocks, mm_cache->object_size);
}

static void inititalize_cache(void)
{
	start_addr = kmalloc(560, GFP_KERNEL);
	
	cache = kmalloc(sizeof(struct mm_cache), GFP_KERNEL);
	cache->object_size = 8;
	cache->num_blocks = 10;
	cache->num_free_blocks = cache->num_blocks;
	cache->free_list = NULL;
	cache->allocated_list = NULL;
	cache->start_addr = start_addr;
	spin_lock_init(&cache->mm_cache_spinlock);
	initialize_slab(cache);
	
	struct mm_cache * cache16b = kmalloc(sizeof(struct mm_cache), GFP_KERNEL);
	cache16b->object_size = 16;
	cache16b->num_blocks = 10;
	cache16b->num_free_blocks = cache16b->num_blocks;
	cache16b->free_list = NULL;
	cache16b->allocated_list = NULL;
	cache16b->start_addr = cache->start_addr + cache16b->object_size*cache16b->num_blocks;
	spin_lock_init(&cache->mm_cache_spinlock);
	initialize_slab(cache16b);
	
	cache->next_mm_cache = cache16b;
	
	struct mm_cache * cache32b = kmalloc(sizeof(struct mm_cache), GFP_KERNEL);
	cache32b->object_size = 32;
	cache32b->num_blocks = 10;
	cache32b->num_free_blocks = cache32b->num_blocks;
	cache32b->free_list = NULL;
	cache32b->allocated_list = NULL;
	cache32b->start_addr = cache16b->start_addr + cache32b->object_size*cache32b->num_blocks;
	spin_lock_init(&cache->mm_cache_spinlock);
	initialize_slab(cache32b);
	
	cache16b->next_mm_cache = cache32b;
	
	printk("SLAB_ALLOCATOR : inititalize_cache()\n");
	
	
}

static int __init mm_slab_init(void)
{
	printk("SLAB_ALLOCATOR : ----------------\nSLAB_ALLOCATOR : mm_slab_init\n");
	inititalize_cache();
	
	return 0;
}

static void __exit mm_slab_exit(void)
{
	kfree(start_addr);
	printk("SLAB_ALLOCATOR : mm_slab_exit\n");
}

module_init(mm_slab_init);
module_exit(mm_slab_exit);

