#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "mm_management.h"
#include <linux/spinlock.h>
#include "error_types.h"

MODULE_LICENSE("Dual BSD/GPL");

struct mm_physical_memory * mem;
struct swap_space * swap_sp;

uintptr_t latest_virtual_address = 0x0000000000000000;	

/*
This function prints the physical addresses of the pages present in free_pages, alloc_pages, pinned_pages list
*/

static void print_list(void)
{
	struct list_head *pos;
	struct mm_page_frame *p_frame;
	
	printk("mm_management : ----------------------------\n");
	printk("mm_management : Free pages----\n");
	list_for_each(pos, &mem->free_pages)
	{
		p_frame = list_entry(pos, struct mm_page_frame, pf_link);
		printk("mm_management : phy addr:%lx\n", p_frame->physical_start_address);
	}
	
	printk("mm_management : Allocated pages----\n");
	list_for_each(pos, &mem->alloc_pages)
	{
		p_frame = list_entry(pos, struct mm_page_frame, pf_link);
		printk("mm_management : phy addr:%lx\n", p_frame->physical_start_address);
	}
	
	printk("mm_management : Pinned pages----\n");
	list_for_each(pos, &mem->pinned_pages)
	{
		p_frame = list_entry(pos, struct mm_page_frame, pf_link);
		printk("mm_management : phy addr:%lx\n", p_frame->physical_start_address);
	}
	printk("mm_management : ----------------------------\n");
}

static void print_swap_space(void)
{
	struct list_head *pos;
	struct swap_block *s_block;
	
	printk("mm_management : ----------------------------\n");
	printk("mm_management : Swap Space Pages----\n");
	list_for_each(pos, &swap_sp->swap_blocks)
	{
		s_block = list_entry(pos, struct swap_block, ss_link);
		printk("mm_management : pid:%d, vfn addr:%lx\n", s_block->pid, s_block->virtual_pframe_addr);
	}
	
	printk("mm_management : ----------------------------\n");
}

/*
This function initializes struct mm_page_frame
Parameters : 
i : the index of page frame in the entire memory.
Returns : pointer of type struct mm_page_frame with initialized values of mm_page_frame
*/

static struct mm_page_frame * pframe_init(int i)
{
	struct mm_page_frame * p_frame;
	
	p_frame = kmalloc(sizeof(struct mm_page_frame), GFP_KERNEL);
	if(!mem)
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
This function gets the requested memory from the Linux kernel which will be acts as the primary memory in the simulator and it divides the primary memory into page frames
*/

static int initialize_memory(void)
{
	mem = kmalloc( sizeof(struct mm_physical_memory), GFP_KERNEL);
	if(!mem)
	{
		printk(KERN_ERR "mm_management : Error allocating struct mm_physical_memory\n");
		return -ERROR_ALLOCATING_MEMORY;
	}
	
	mem->memory_addr_start = (uintptr_t)kmalloc( TOTAL_MEMORY_EXP, GFP_KERNEL);
	if(!mem->memory_addr_start)
	{
		printk(KERN_ERR "mm_management : Error allocating requested memory\n");
		return -ERROR_ALLOCATING_MEMORY;
	}
	mem->total_pages = TOTAL_MEMORY_EXP / PAGE_SIZE_EXP;
	
	INIT_LIST_HEAD(&mem->free_pages);
	INIT_LIST_HEAD(&mem->alloc_pages);
	INIT_LIST_HEAD(&mem->pinned_pages);
	
	mutex_init(&mem->mm_memory_mutex);
	
	struct mm_page_frame * p_frame;
	
	for(int i=0; i < mem->total_pages-1; i++)
	{
		p_frame = pframe_init(i);
		if(!p_frame)
		{
			return -ERROR_ALLOCATING_MEMORY;
		}
		list_add_tail(&p_frame->pf_link, &mem->free_pages);
	}
	
	p_frame = pframe_init(mem->total_pages-1);
	if(!p_frame)
	{
		return -ERROR_ALLOCATING_MEMORY;
	}
	list_add_tail(&p_frame->pf_link, &mem->pinned_pages);
	
	mem->cr3_page_table_addr = p_frame->physical_start_address;
	
	swap_sp = kmalloc( sizeof(struct swap_space), GFP_KERNEL);
	INIT_LIST_HEAD(&swap_sp->swap_blocks);
	mutex_init(&swap_sp->swap_space_mutex);
	
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

static int get_multilevel_pagetables(uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t * next_page_addr)
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

static int invalidate_PTE(uintptr_t virtual_page_address)
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
This function moves the allocated page into free page list and copies the data in the allocated page into space
*/

static int swap_page(void)
{
	struct swap_block * swap_block = kmalloc( sizeof(struct swap_block), GFP_KERNEL);
	if(!swap_block)
	{
		printk(KERN_ERR "mm_management : Error allocating struct swap_block\n");
		return -ERROR_ALLOCATING_MEMORY;
	}
	
	while(1)
	{
		if( mutex_trylock(&mem->mm_memory_mutex) )
		{
			if( mutex_trylock(&swap_sp->swap_space_mutex) )
			{
				break;
			}
			mutex_unlock(&mem->mm_memory_mutex);
		}
		
		// Improv : Try adding sleeping mechanism or yeild the CPU
	}
	
	list_add_tail(&swap_block->ss_link, &swap_sp->swap_blocks);
	
	if(list_empty(&mem->alloc_pages))
	{
		mutex_unlock(&swap_sp->swap_space_mutex);
		mutex_unlock(&mem->mm_memory_mutex);
		printk(KERN_ERR "mm_management : No page frames available in the memory\n");
		return -NO_PAGE_FRAME_AVAILABLE;
	}
	
	struct mm_page_frame * p_frame = list_first_entry(&mem->alloc_pages, struct mm_page_frame, pf_link);
	
	swap_block->virtual_pframe_addr = p_frame->virtual_start_address;
	swap_block->pid = p_frame->pid;
	swap_block->data = kmalloc( PAGE_SIZE_EXP, GFP_KERNEL);
	
	memcpy(swap_block->data, (void *)p_frame->physical_start_address, PAGE_SIZE_EXP);
	
	int err = invalidate_PTE(p_frame->virtual_start_address);
	
	if(err)
	{
		mutex_unlock(&swap_sp->swap_space_mutex);
		mutex_unlock(&mem->mm_memory_mutex);
		return err;
	}
	
	list_move_tail(&p_frame->pf_link, &mem->free_pages);
	
	mutex_unlock(&swap_sp->swap_space_mutex);
	mutex_unlock(&mem->mm_memory_mutex);
	
	printk("mm_management : PAGE_SWAP : Page frame addr:%lx\n", p_frame->physical_start_address);
	
	return 0;
}

static int get_swap_space_data(void * meta_data)
{
	struct swap_meta_data * m_data = (struct swap_meta_data *)meta_data;
	struct swap_block *s_block, *temp;
	struct mm_page_frame * p_frame;
	
	printk("DEBUG : m_data->pid:%d, m_data->virtual_pframe_addr:%lx\n", m_data->pid, m_data->virtual_pframe_addr);
	
	list_for_each_entry_safe(s_block, temp, &swap_sp->swap_blocks, ss_link)
	{
		if(s_block->pid == m_data->pid && s_block->virtual_pframe_addr == m_data->virtual_pframe_addr )
		{
			p_frame = get_free_page_internal(0);
			
			int err = update_page_table(m_data->virtual_pframe_addr, p_frame->physical_start_address);
			
			if(err)
			{
				// Improv : call free_page
				return err;
			}
			else
			{
				mutex_lock(&mem->mm_memory_mutex);
				
				p_frame->virtual_start_address = m_data->virtual_pframe_addr;
				p_frame->pid = m_data->pid;
				memcpy((void *)p_frame->physical_start_address, s_block->data, PAGE_SIZE_EXP);
				
				mutex_unlock(&mem->mm_memory_mutex);
			}
			
			list_del(&s_block->ss_link);
			
			kfree(s_block->data);
			kfree(s_block);
			
			return 0;
		}
	}
	
	return -SWAP_SPACE_ERROR;
}

/*
This page handles page_fault when the pages are not available
*/

static int handle_page_fault(int cmd, void * data)
{
	int err;
	
	switch(cmd)
	{
		case PAGE_FAULT_NO_PAGE:	err = swap_page();
						if(err)
						{
							return err;
						}
						break;
						
		case PAGE_FAULT_INVALID_PTE :	err = get_swap_space_data(data);
						if(err)
						{
							return err;
						}
						break;
	}
	
	return 0;
}



/*
This function moves the page from free page list to allocated list or pinned list based om the pinned_page_flag
If pinned_page_flag is set then it moved the page from freepage list to pinned list else to allocated list
It returns the pointer of the pframe that was moved to allocated or pinned list
If it returns NULL then there is no free page available
*/

static struct mm_page_frame * get_free_page_internal(bool pinned_page_flag)
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

/*
This function sets the page table entries
pfn : 52 bit physical page frame number or physical page frame starting address >> 12
returns uintptr_t with it reference and validity bit set(i.e., 54 and 53 bits)
*/

static inline uintptr_t set_PTE(uintptr_t pfn)
{
	return pfn | 0x0030000000000000;
}


/*
This function sets the page table entries
pfn : 52 bit physical page frame number or physical page frame starting address >> 12
returns uintptr_t with it reference and validity bit set(i.e., 54 and 53 bits)
*/

static inline uintptr_t set_PTE_Reference_bit(uintptr_t pfn)
{
	return pfn | 0x0020000000000000;
}

/*
This function updates the multilevel page tables
Paramters :
vfn : 52 bit virtual page frame number
level : page table level (1 - 4)
*/

static int update_multilevel_pagetables(uintptr_t vfn, uintptr_t level, uintptr_t page_table_addr, uintptr_t page_frame_physical_addr, uintptr_t * next_page_addr)
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
			struct mm_page_frame * p_frame = get_free_page_internal(1);
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

static int update_page_table(uintptr_t virtual_address, uintptr_t page_frame_physical_addr)
{
	//printk("DEBUG : update_page_table\n");
	uintptr_t vfn = virtual_address >> 12;
	uintptr_t page_table_addr;
	int err;
	
	//printk("DEBUG : pagetableaddr:%lx\n", mem->cr3_page_table_addr);
	err = update_multilevel_pagetables( vfn, 1, mem->cr3_page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	//page_table_addr = next_page_table_addr;
	
	//printk("DEBUG : pagetableaddr:%lx\n", page_table_addr);
	err = update_multilevel_pagetables( vfn, 2, page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	//page_table_addr = next_page_table_addr;
	
	//printk("DEBUG : pagetableaddr:%lx\n", page_table_addr);
	err = update_multilevel_pagetables( vfn, 3, page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	//page_table_addr = next_page_table_addr;
	
	//printk("DEBUG : pagetableaddr:%lx\n", page_table_addr);
	err = update_multilevel_pagetables( vfn, 4, page_table_addr, page_frame_physical_addr, &page_table_addr);
	if(err)
	{
		return err;
	}
	
	return 0;
	
}

/*
This function requests an page to the kernel
Returns the starting virtual address of the page frame
*/

static int get_free_page(uintptr_t * addr)
{
	//printk("DEBUG : get_free_page\n");
	struct mm_page_frame * p_frame = get_free_page_internal(0);
	
	if(!p_frame)
	{
		return -1;
	}
	int err = update_page_table(latest_virtual_address, p_frame->physical_start_address);
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

static int virtual_to_physical_address(uintptr_t virtual_address, uintptr_t * physical_addr)
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

static int free_page_internal(uintptr_t physical_addr, bool pinned_page_flag)
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

static int mm_free_page(uintptr_t virtual_addr)
{
	int err;
	
	uintptr_t physical_addr;
	
	err = virtual_to_physical_address(virtual_addr, &physical_addr);
	if(err)
	{
		return err;
	}
	
	err = invalidate_PTE(virtual_addr);
	if(err)
	{
		return err;
	}
	
	err = free_page_internal(physical_addr , 0);
	if(err)
	{
		return err;
	}
	return 0;
}

static int __init mm_management_init(void)
{
	
	printk("mm_management : \nmm_management : Start ---------------------\n");
	
	if(initialize_memory())
	{
		return -1;
	}
	print_list();
	
	uintptr_t addr1, p_addr1;
	printk("mm_management : ---------------------------\n");
	int err = get_free_page(&addr1);
	
	if(!err)
	{
		printk("mm_management : addr1:%lx\n", addr1);
	}
	
	virtual_to_physical_address(addr1, &p_addr1);
	printk("mm_management : physical address adr1:%lx\n", p_addr1);
	
	err = mm_free_page(addr1);
	
	print_list();
	
	return 0;
}

/*
Frees the memory that was given by the Linux
*/

static int uninitialize_memory(void)
{
	struct mm_page_frame * p_frame, * temp_p_frame;
	
	list_for_each_entry_safe(p_frame, temp_p_frame, &mem->free_pages, pf_link)
	{
		list_del(&p_frame->pf_link);
		kfree(p_frame);
	}
	
	if(!list_empty(&mem->free_pages))
	{
		printk(KERN_ERR "mm_management : All pages are not free'd. Free pages are present in kernel\n");
		return -ERROR_DEALLOCATING_MEMORY;
	}
	
	list_for_each_entry_safe(p_frame, temp_p_frame, &mem->alloc_pages, pf_link)
	{
		list_del(&p_frame->pf_link);
		kfree(p_frame);
	}
	
	if(!list_empty(&mem->alloc_pages))
	{
		printk(KERN_ERR "mm_management : All pages are not free'd. Allocated pages are present in kernel\n");
		return -ERROR_DEALLOCATING_MEMORY;
	}
	
	list_for_each_entry_safe(p_frame, temp_p_frame, &mem->pinned_pages, pf_link)
	{
		list_del(&p_frame->pf_link);
		kfree(p_frame);
	}
	
	if(!list_empty(&mem->pinned_pages))
	{
		printk(KERN_ERR "mm_management : All pages are not free'd. Pinned pages are present in kernel\n");
		return -ERROR_DEALLOCATING_MEMORY;
	}
	
	mutex_destroy(&mem->mm_memory_mutex);
	
	kfree((void *)mem->memory_addr_start);
	kfree(mem);
	
	return 0;
}

static void __exit mm_management_exit(void)
{
	uninitialize_memory();
	printk("mm_management : mm_management_exit\n");
}

module_init(mm_management_init);
module_exit(mm_management_exit);

