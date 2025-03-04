#include "mm_page_frame.h"

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

extern struct swap_space * swap_sp;

void initialise_swap_space(void);
void print_swap_space(void);
int swap_page(struct mm_physical_memory *);
int handle_page_fault(struct mm_physical_memory *, int cmd, void * data);
int get_swap_space_data(struct mm_physical_memory *, void * meta_data);

