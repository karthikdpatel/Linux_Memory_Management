//#DEFINE TOTAL_MEMORY (10*1024*1024)
#define TOTAL_MEMORY_EXP (20*1024)
#define PAGE_SIZE_EXP (4*1024)

#define PF_DIRTY 0x01
#define PF_BUSY 0x02
#define PF_PINNED 0x04

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

struct mm_page_frame
{
	uintptr_t physical_start_address;
	size_t size;

	struct list_head pf_link; // link on free,allocated and pinned list
	struct list_head pf_scheduler_link; // link on active, inactive scheduler lists
	
	uint8_t pf_flags; // PF_DIRTY, PF_BUSY, PF_PINNED;
	
	uintptr_t virtual_start_address; // used for reverse mapping
	pid_t pid; // used for reverse mapping
};

static void print_list(void);
static void print_swap_space(void);
static struct mm_page_frame * pframe_init(int i);
static int initialize_memory(void);
static int get_multilevel_pagetables(uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t * next_page_addr);
static int invalidate_PTE(uintptr_t virtual_page_address);
static int swap_page(void);
static int handle_page_fault(int cmd, void * data);
static struct mm_page_frame * get_free_page_internal(bool pinned_page_flag);
static inline uintptr_t set_PTE(uintptr_t pfn);
static inline uintptr_t set_PTE_Reference_bit(uintptr_t pfn);
static int update_multilevel_pagetables(uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t page_frame_physical_addr, uintptr_t * next_page_addr);
static int update_page_table(uintptr_t virtual_address, uintptr_t page_frame_physical_addr);
static int get_free_page(uintptr_t * addr);
static int virtual_to_physical_address(uintptr_t virtual_address, uintptr_t * physical_addr);
static int uninitialize_memory(void);


#define PAGE_FAULT_NO_PAGE 0x01
#define PAGE_FAULT_INVALID_PTE 0x02

struct swap_space
{
	struct list_head swap_blocks;
	struct mutex swap_space_mutex;
};

struct swap_block
{
	struct list_head ss_link; // link to swap_space.swap_blocks
	uintptr_t virtual_pframe_addr;
	pid_t pid;
	void * data;
};


struct swap_meta_data
{
	pid_t pid;
	uintptr_t virtual_pframe_addr;
};


