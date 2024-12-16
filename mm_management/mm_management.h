//#DEFINE TOTAL_MEMORY (10*1024*1024)
#define TOTAL_MEMORY_EXP (128*1024)
#define PAGE_SIZE_EXP (4*1024)

struct mm_memory
{
	void * userspace_memory_addr;
	void * kernel_memory_addr;
	
	uintptr_t num_user_space_pages;
	uintptr_t num_free_user_space_pages;
	
	uintptr_t num_kernel_pages;
	uintptr_t num_free_kernel_pages;
	
	struct list_head user_space_free_pages_list;
	struct list_head user_space_alloc_pages_list;
	
	struct list_head kernel_free_pages_list;
	struct list_head kernel_alloc_pages_list;
	
	struct mutex mm_memory_mutex;
	
	void * cr3_page_table_addr;
};

struct page_frame
{
	void * physical_start_addr;
	uintptr_t physical_page_frame_num;
	size_t size;
	bool free;
	struct list_head mm_memory_list;
};

struct mm_memory_page
{
	uintptr_t page_num;
};

struct page_map_table
{
	uintptr_t virtual_page_frame_num;
	//struct page_table_entry[512];
};

struct page_directory_pointer_table
{
	uintptr_t virtual_page_frame_num;
	//struct page_table_entry[512];
};

struct page_directory_table
{
	uintptr_t virtual_page_frame_num;
	//struct page_table_entry[512];
};

struct page_table
{
	uintptr_t virtual_page_frame_num;
	//struct page_table_entry[512];
};

struct page_table_entry
{
	uintptr_t page_frame_num;
	bool validity;
	bool modified;
};


