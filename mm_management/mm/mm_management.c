#include "../include/mm_management.h"

/*
void print_list(void)
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
*/

/*
This function gets the requested memory from the Linux kernel which will be acts as the primary memory in the simulator and it divides the primary memory into page frames
*/

int initialize_memory(struct mm_physical_memory * mem)
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
	
	return 0;	
}

/*
Frees the memory that was given by the Linux
*/

/*int uninitialize_memory(void)
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
}*/


