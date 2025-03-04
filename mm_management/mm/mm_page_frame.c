#include "../include/mm_page_frame.h"

int handle_page_fault(int cmd, void * data);

uintptr_t latest_virtual_address = 0x0000000000000000;

int initialize_pframes(struct mm_physical_memory * mem)
{
	struct mm_page_frame * p_frame;
	
	for(int i=0; i < mem->total_pages-1; i++)
	{
		p_frame = pframe_init(i, mem);
		if(!p_frame)
		{
			return -ERROR_ALLOCATING_MEMORY;
		}
		list_add_tail(&p_frame->pf_link, &mem->free_pages);
	}
	
	p_frame = pframe_init(mem->total_pages-1, mem);
	if(!p_frame)
	{
		return -ERROR_ALLOCATING_MEMORY;
	}
	list_add_tail(&p_frame->pf_link, &mem->pinned_pages);
	
	mem->cr3_page_table_addr = p_frame->physical_start_address;
	return 0;
}


/*
This function initializes struct mm_page_frame
Parameters : 
i : the index of page frame in the entire memory.
Returns : pointer of type struct mm_page_frame with initialized values of mm_page_frame
*/

struct mm_page_frame * pframe_init(int i, struct mm_physical_memory * mem)
{
	struct mm_page_frame * p_frame;
	
	p_frame = kmalloc(sizeof(struct mm_page_frame), GFP_KERNEL);
	if(!p_frame)
	{
		printk(KERN_ERR "mm_management : Error allocating struct mm_page_frame\n");
		return NULL;
	}
	p_frame->physical_start_address = mem->memory_addr_start + i*PAGE_SIZE_EXP;
	p_frame->size = PAGE_SIZE_EXP;
	p_frame->pf_flags = 0;
	
	return p_frame;
}



/*
This function moves the page from free page list to allocated list or pinned list based om the pinned_page_flag
If pinned_page_flag is set then it moved the page from freepage list to pinned list else to allocated list
It returns the pointer of the pframe that was moved to allocated or pinned list
If it returns NULL then there is no free page available
*/

struct mm_page_frame * get_free_page_internal(struct mm_physical_memory * mem, bool pinned_page_flag)
{
	//printk("DEBUG : get_free_page_internal\n");
	struct mm_page_frame * p_frame;
	int err;
	mutex_lock(&mem->mm_memory_mutex);
	
	if(list_empty(&mem->free_pages))
	{
		mutex_unlock(&mem->mm_memory_mutex);
		err = handle_page_fault(PAGE_FAULT_NO_PAGE, 0);
		if(err)
		{
			return NULL;
		}
	}
	
	if(pinned_page_flag)
	{
		p_frame = list_first_entry(&mem->free_pages, struct mm_page_frame, pf_link);
		list_move_tail(&p_frame->pf_link, &mem->pinned_pages);
		p_frame->pf_flags = p_frame->pf_flags | PF_PINNED;
		printk("mm_management : PAGE ALLOCATION : Pinned page frame addr:%lx\n",p_frame->physical_start_address);
	}
	else
	{
		p_frame = list_first_entry(&mem->free_pages, struct mm_page_frame, pf_link);
		list_move_tail(&p_frame->pf_link, &mem->alloc_pages);
		printk("mm_management : PAGE ALLOCATION : Allocated page frame addr:%lx\n",p_frame->physical_start_address);
	}
	
	mutex_unlock(&mem->mm_memory_mutex);
	
	return p_frame;
}


int free_page_internal(struct mm_physical_memory * mem, uintptr_t physical_addr, bool pinned_page_flag)
{
	if( (physical_addr & 0x0000000000000FFF) != (0x000) )
	{
		printk(KERN_ERR "mm_management : Invalid address given to free_page_internal(), addr:%lx\n", physical_addr);
		return -INVALID_INPUT;
	}

	struct mm_page_frame *p_frame, *temp_p_frame;
	
	mutex_lock(&mem->mm_memory_mutex);
	
	if(pinned_page_flag)
	{	
		if(list_empty(&mem->pinned_pages))
		{
			printk(KERN_ERR "mm_management : No Page available in the pinned list to free\n");
			mutex_unlock(&mem->mm_memory_mutex);
			return -NO_PAGE_FRAME_AVAILABLE;
		}
		
		list_for_each_entry_safe(p_frame, temp_p_frame, &mem->pinned_pages, pf_link)
		{
			if(p_frame->physical_start_address == physical_addr)
			{
				list_move_tail(&p_frame->pf_link, &mem->free_pages);
				mutex_unlock(&mem->mm_memory_mutex);
				return 0;
			}
		}
		
		printk(KERN_ERR "mm_management : Given page is not available in the pinned list\n");
	}
	else
	{
		if(list_empty(&mem->alloc_pages))
		{
			printk(KERN_ERR "mm_management : No Page available in the allocated list to free\n");
			mutex_unlock(&mem->mm_memory_mutex);
			return -NO_PAGE_FRAME_AVAILABLE;
		}
		
		list_for_each_entry_safe(p_frame, temp_p_frame, &mem->alloc_pages, pf_link)
		{
			if(p_frame->physical_start_address == physical_addr)
			{
				list_move_tail(&p_frame->pf_link, &mem->free_pages);
				mutex_unlock(&mem->mm_memory_mutex);
				return 0;
			}
		}
		
		printk(KERN_ERR "mm_management : Given page is not available in the pinned list\n");
	}
	
	mutex_unlock(&mem->mm_memory_mutex);
	return -NO_PAGE_FRAME_AVAILABLE;
}


/*
This function sets the page table entries
pfn : 52 bit physical page frame number or physical page frame starting address >> 12
returns uintptr_t with it reference and validity bit set(i.e., 54 and 53 bits)
*/

inline uintptr_t set_PTE(uintptr_t pfn)
{
	return pfn | 0x0030000000000000;
}


/*
This function sets the page table entries
pfn : 52 bit physical page frame number or physical page frame starting address >> 12
returns uintptr_t with it reference and validity bit set(i.e., 54 and 53 bits)
*/

inline uintptr_t set_PTE_Reference_bit(uintptr_t pfn)
{
	return pfn | 0x0020000000000000;
}


/*
This function requests an page to the kernel
Returns the starting virtual address of the page frame
*/

int get_free_page(struct mm_physical_memory * mem, uintptr_t * addr)
{
	//printk("DEBUG : get_free_page\n");
	struct mm_page_frame * p_frame = get_free_page_internal(mem, 0);
	
	if(!p_frame)
	{
		return -1;
	}
	int err = update_page_table(mem, latest_virtual_address, p_frame->physical_start_address);
	if(err)
	{
		// Improv : call free_page
		return -1;
	}
	else
	{
		p_frame->virtual_start_address = latest_virtual_address;
		p_frame->pid = current->pid;
		
		//printk("DEBUG : p_frame->virtual_start_address:%lx, p_frame->pid:%d\n", p_frame->virtual_start_address, p_frame->pid);
		
		(* addr) = latest_virtual_address;
		latest_virtual_address += (0x01000);
		return 0;
	}
}


/*
This function converts given virtual address to physical address
Paramters:
virtual_address : virtual address value
(* physical_addr) : physical address will be stored in this variable and returned back
*/

int virtual_to_physical_address(struct mm_physical_memory * mem, uintptr_t virtual_address, uintptr_t * physical_addr)
{
	uintptr_t vfn = virtual_address >> 12;
	uintptr_t page_table_addr;
	
	int err;
	
	err = get_multilevel_pagetables(vfn, 1, mem->cr3_page_table_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	err = get_multilevel_pagetables(vfn, 2, page_table_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	err = get_multilevel_pagetables(vfn, 3, page_table_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	err = get_multilevel_pagetables(vfn, 4, page_table_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	*physical_addr = page_table_addr;
	
	return 0;
}

/*
This function gets the page table addresses of the multilevel page tables and at the 4th level gets the physical page address
Parameters :
vfn : virtual frame number
level : page table level (1-4)
page_table_addr : current page table address
* next_page_addr : returns next page table address or at the last level returns the physical page address

*/

int get_multilevel_pagetables(uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t * next_page_addr)
{
	//printk("DEBUG : get_multilevel_pagetables\n");
	uintptr_t ind;
	int err;
	
	switch(level)
	{
		case 1: ind = (vfn &  0xFF8000000) >> 27; //Extract 27-35 bits of vfn
			break;
		
		case 2: ind = (vfn &  0x07FC0000) >> 18; //Extract 18-26 bits of vfn
			break;
			
		case 3: ind = (vfn &  0x03FE00) >> 9; //Extract 9-17 bits of vfn
			break;
			
		case 4: ind = (vfn &  0x01FF); //Extract 0-8 bits of vfn
			break;
	}
	
	uintptr_t * pte_address = (uintptr_t *)((void *)page_table_addr + ind*4); // get PTE address
	
	if( !(*pte_address & 0x010000000000000) ) // Improv :  check if the PTE entry has a valid physical address
	{
		struct swap_meta_data meta_data = {
			.pid = current->pid,
			.virtual_pframe_addr = vfn << 12,
		};
		
		err = handle_page_fault(PAGE_FAULT_INVALID_PTE, &meta_data);
		if(err)
		{
			printk(KERN_ERR "mm_management : handlepagefault, err:%d\n", err);
			return err;
		}
	}
	*next_page_addr = (*pte_address & 0x000FFFFFFFFFFFFF) << 12;
	
	return 0;
}


/*
This function sets the validity bit of the PTE of the given virtual address to 0(invalid PTE)
*/

int invalidate_PTE(struct mm_physical_memory * mem, uintptr_t virtual_page_address)
{
	int err;
	uintptr_t page_table_addr;
	uintptr_t vfn = virtual_page_address >> 12;
	
	err = get_multilevel_pagetables(vfn, 1, mem->cr3_page_table_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	err = get_multilevel_pagetables(vfn, 2, page_table_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	err = get_multilevel_pagetables(vfn, 3, page_table_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	uintptr_t ind = (vfn &  0x01FF);
	uintptr_t * pte_address = (uintptr_t *)((void *)page_table_addr + ind*4);
	
	if( !(*pte_address & 0x010000000000000) )
	{
		printk(KERN_ERR "mm_management : Wrong PTE value found while invalidating the PTE\n");
		return -WRONG_VALUE;
	}
	else
	{
		*pte_address = *pte_address & 0xFFEFFFFFFFFFFFFF;
	}
	
	printk("mm_management : PAGE_SWAP : INVALIDATED PTE : PTE value: %lx\n", *pte_address);
	
	return 0;
}


/*
This function updates the multilevel page tables
Paramters :
vfn : 52 bit virtual page frame number
level : page table level (1 - 4)
*/

int update_multilevel_pagetables(struct mm_physical_memory * mem, uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t page_frame_physical_addr, uintptr_t * next_page_addr)
{
	//printk("DEBUG : update_multilevel_pagetables\n");
	uintptr_t ind;
	
	switch(level)
	{
		case 1: ind = (vfn &  0xFF8000000) >> 27; //Extract 27-35 bits of vfn
			break;
		
		case 2: ind = (vfn &  0x07FC0000) >> 18; //Extract 18-26 bits of vfn
			break;
			
		case 3: ind = (vfn &  0x03FE00) >> 9; //Extract 9-17 bits of vfn
			break;
			
		case 4: ind = (vfn &  0x01FF); //Extract 0-8 bits of vfn
			break;
	}
	
	uintptr_t * pte_address = (uintptr_t *)((void *)page_table_addr + ind*4); // get PTE address
	
	if( !(*pte_address & 0x010000000000000) ) // check if the PTE entry has a valid physical address
	{
		if(level == 1 || level == 2 || level == 3) //
		{
			struct mm_page_frame * p_frame = get_free_page_internal(mem, 1);
			if(!p_frame)
			{
				return -NO_PAGE_FRAME_AVAILABLE;
			}
			
			*pte_address = set_PTE( p_frame->physical_start_address >> 12 );
			
		}
		else
		{
			*pte_address = set_PTE( page_frame_physical_addr >> 12 );
		}
	}
	else
	{
		*pte_address = set_PTE_Reference_bit(*pte_address);
	}
	*next_page_addr = (*pte_address & 0x000FFFFFFFFFFFFF) << 12;
	
	return 0;
}


int update_page_table(struct mm_physical_memory * mem, uintptr_t virtual_address, uintptr_t page_frame_physical_addr)
{
	//printk("DEBUG : update_page_table\n");
	uintptr_t vfn = virtual_address >> 12;
	uintptr_t page_table_addr;
	int err;
	
	//printk("DEBUG : pagetableaddr:%lx\n", mem->cr3_page_table_addr);
	err = update_multilevel_pagetables(mem, vfn, 1, mem->cr3_page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	//page_table_addr = next_page_table_addr;
	
	//printk("DEBUG : pagetableaddr:%lx\n", page_table_addr);
	err = update_multilevel_pagetables(mem, vfn, 2, page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	//page_table_addr = next_page_table_addr;
	
	//printk("DEBUG : pagetableaddr:%lx\n", page_table_addr);
	err = update_multilevel_pagetables(mem, vfn, 3, page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	//page_table_addr = next_page_table_addr;
	
	//printk("DEBUG : pagetableaddr:%lx\n", page_table_addr);
	err = update_multilevel_pagetables(mem, vfn, 4, page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	return 0;
	
}


int mm_free_page(struct mm_physical_memory * mem, uintptr_t virtual_addr)
{
	int err;
	
	uintptr_t physical_addr;
	
	err = virtual_to_physical_address(mem, virtual_addr, &physical_addr);
	if(err)
	{
		return err;
	}
	
	err = invalidate_PTE(mem, virtual_addr);
	if(err)
	{
		return err;
	}
	
	err = free_page_internal(mem, physical_addr , 0);
	if(err)
	{
		return err;
	}
	return 0;
}




