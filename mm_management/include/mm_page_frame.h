#include <linux/sched.h>
#include "mm_management.h"

#define PAGE_FAULT_NO_PAGE 0x01
#define PAGE_FAULT_INVALID_PTE 0x02


extern uintptr_t latest_virtual_address;

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

struct swap_meta_data
{
	pid_t pid;
	uintptr_t virtual_pframe_addr;
};

int initialize_pframes(struct mm_physical_memory *);

struct mm_page_frame * pframe_init(int i, struct mm_physical_memory *);
struct mm_page_frame * get_free_page_internal(struct mm_physical_memory *, bool pinned_page_flag);
inline uintptr_t set_PTE(uintptr_t pfn);
inline uintptr_t set_PTE_Reference_bit(uintptr_t pfn);
int get_free_page(struct mm_physical_memory * mem, uintptr_t * addr);
int free_page_internal(struct mm_physical_memory *, uintptr_t physical_addr, bool pinned_page_flag);
int mm_free_page(struct mm_physical_memory * mem, uintptr_t virtual_addr);

int virtual_to_physical_address(struct mm_physical_memory *, uintptr_t virtual_address, uintptr_t * physical_addr);

int get_multilevel_pagetables(uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t * next_page_addr);
int invalidate_PTE(struct mm_physical_memory *, uintptr_t virtual_page_address);
int update_multilevel_pagetables(struct mm_physical_memory * mem, uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t page_frame_physical_addr, uintptr_t * next_page_addr);
int update_page_table(struct mm_physical_memory *, uintptr_t virtual_address, uintptr_t page_frame_physical_addr);

