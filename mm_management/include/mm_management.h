#include <linux/slab.h>
#include <linux/spinlock.h>
#include "../error_types.h"

//#DEFINE TOTAL_MEMORY (10*1024*1024)
#define TOTAL_MEMORY_EXP (20*1024)
#define PAGE_SIZE_EXP (4*1024)

#define PF_DIRTY 0x01
#define PF_BUSY 0x02
#define PF_PINNED 0x04

struct mm_page_frame;

struct mm_physical_memory
{
	uintptr_t memory_addr_start;
	uintptr_t total_pages;
	
	uintptr_t cr3_page_table_addr;
	
	struct list_head free_pages;
	struct list_head alloc_pages;
	struct list_head pinned_pages;
	
	struct list_head active_pages;
	struct list_head in_active_pages;
	
	struct mutex mm_memory_mutex;
};




//void print_list(void);
int initialize_memory(struct mm_physical_memory *);
//int uninitialize_memory(void);


