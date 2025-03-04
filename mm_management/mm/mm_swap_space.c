#include "../include/mm_swap_space.h"

struct swap_space * swap_sp = NULL;

void initialise_swap_space(void)
{
	swap_sp = kmalloc( sizeof(struct swap_space), GFP_KERNEL);
	INIT_LIST_HEAD(&swap_sp->swap_blocks);
	mutex_init(&swap_sp->swap_space_mutex);
}

void print_swap_space(void)
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
This function moves the allocated page into free page list and copies the data in the allocated page into space
*/

int swap_page(struct mm_physical_memory * mem)
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
		
		// Improve : Try adding sleeping mechanism or yeild the CPU
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
	
	int err = invalidate_PTE(mem, p_frame->virtual_start_address);
	
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


/*
This page handles page_fault when the pages are not available
*/

int handle_page_fault(struct mm_physical_memory * mem, int cmd, void * data)
{
	int err;
	
	switch(cmd)
	{
		case PAGE_FAULT_NO_PAGE:	err = swap_page(mem);
						if(err)
						{
							return err;
						}
						break;
						
		case PAGE_FAULT_INVALID_PTE :	err = get_swap_space_data(mem, data);
						if(err)
						{
							return err;
						}
						break;
	}
	
	return 0;
}


int get_swap_space_data(struct mm_physical_memory * mem, void * meta_data)
{
	struct swap_meta_data * m_data = (struct swap_meta_data *)meta_data;
	struct swap_block *s_block, *temp;
	struct mm_page_frame * p_frame;
	
	printk("DEBUG : m_data->pid:%d, m_data->virtual_pframe_addr:%lx\n", m_data->pid, m_data->virtual_pframe_addr);
	
	list_for_each_entry_safe(s_block, temp, &swap_sp->swap_blocks, ss_link)
	{
		if(s_block->pid == m_data->pid && s_block->virtual_pframe_addr == m_data->virtual_pframe_addr )
		{
			p_frame = get_free_page_internal(mem, 0);
			
			int err = update_page_table(mem, m_data->virtual_pframe_addr, p_frame->physical_start_address);
			
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



