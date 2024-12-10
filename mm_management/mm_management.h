//#DEFINE TOTAL_MEMORY (10*1024*1024)
#define TOTAL_MEMORY_EXP (20*1024)
#define PAGE_SIZE_EXP (4*1024)

struct mm_memory
{
	unsigned long num_total_pages;
	unsigned long num_free_pages;
	struct list_head free_pages_list;
	struct list_head alloc_pages_list;
	
	//spinlock_t mm_memory_spinlock;
	//unsigned long mm_memory_spinlock_irq_flag;
	
	struct mutex mm_memory_mutex;
};

struct page_frame
{
	void * physical_start_addr;
	size_t size;
	uintptr_t physical_page_frame_num;
	bool validity;
	bool modified;
	struct list_head list;
};


