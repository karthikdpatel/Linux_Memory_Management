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

void * memory_addr;

struct mm_memory * mem;
uintptr_t latest_virtual_p_frame_addr_usr_space = 0x0000000000000000;
uintptr_t latest_virtual_p_frame_addr_kernel_space = 0xFFFF800000000000;

static int initialize_memory(void)
{
	memory_addr = kmalloc(TOTAL_MEMORY_EXP, GFP_KERNEL);
	if(!memory_addr)
	{
		printk(KERN_ERR "mm_management : Error in getting the total memory\n");
		return -1;
	}
	
	mem = kmalloc(sizeof(struct mm_memory), GFP_KERNEL);
	if(!mem)
	{
		printk(KERN_ERR "mm_management : Error in allocating memory structure\n");
		return -1;
	}
	
	mem->num_user_space_pages = (TOTAL_MEMORY_EXP * 3) / (PAGE_SIZE_EXP * 4) ;
	mem->num_free_user_space_pages = mem->num_user_space_pages;
	
	//printk("mm_management : Number of user space pages:%lx\n", mem->num_user_space_pages);
	
	mem->num_kernel_pages = (TOTAL_MEMORY_EXP / (PAGE_SIZE_EXP * 4));
	mem->num_free_kernel_pages = mem->num_kernel_pages;
	
	//printk("mm_management : Number of user space pages:%lx\n", mem->num_kernel_pages);
	
	INIT_LIST_HEAD(&mem->user_space_free_pages_list);
	INIT_LIST_HEAD(&mem->user_space_alloc_pages_list);
	
	INIT_LIST_HEAD(&mem->kernel_free_pages_list);
	INIT_LIST_HEAD(&mem->kernel_alloc_pages_list);
	
	mutex_init(&mem->mm_memory_mutex);
	
	mem->userspace_memory_addr = memory_addr;
	
	int i;
	struct page_frame *p_frame;
	
	for(i=0; i < mem->num_user_space_pages; i++)
	{
		p_frame = kmalloc(sizeof(struct page_frame), GFP_KERNEL);
		if(!p_frame)
		{
			printk(KERN_ERR "mm_management : Error in allocating pageframe structure\n");
			return -1;
		}
		
		p_frame->physical_start_addr = memory_addr + i*PAGE_SIZE_EXP;
		p_frame->size = PAGE_SIZE_EXP;
		p_frame->physical_page_frame_num = ((uintptr_t)p_frame->physical_start_addr >> 12);
		
		list_add_tail(&p_frame->mm_memory_list, &mem->user_space_free_pages_list);
		p_frame->free = true;
	}
	
	mem->kernel_memory_addr = memory_addr + mem->num_user_space_pages * PAGE_SIZE_EXP;
	
	for(i=0 ; i < mem->num_kernel_pages; i++)
	{
		p_frame = kmalloc(sizeof(struct page_frame), GFP_KERNEL);
		if(!p_frame)
		{
			printk(KERN_ERR "mm_management : Error in allocating pageframe structure\n");
			return -1;
		}
		
		p_frame->physical_start_addr = mem->kernel_memory_addr + i*PAGE_SIZE_EXP;
		p_frame->size = PAGE_SIZE_EXP;
		p_frame->physical_page_frame_num = ((uintptr_t)p_frame->physical_start_addr >> 12);
		
		if(i != mem->num_kernel_pages-1)
		{
			list_add_tail(&p_frame->mm_memory_list, &mem->kernel_free_pages_list);
			p_frame->free = true;
		}
		else
		{
			list_add_tail(&p_frame->mm_memory_list, &mem->kernel_alloc_pages_list);
			p_frame->free = false;
			mem->cr3_page_table_addr = p_frame->physical_start_addr;
		}
		
	}
	
	
	return 0;
}

static void check_free_memory(void)
{
	struct list_head *pos;
	struct page_frame *p_frame;
	
	printk("mm_management : Free Pages User Space--\n");
	list_for_each(pos, &mem->user_space_free_pages_list)
	{
		p_frame = list_entry(pos, struct page_frame, mm_memory_list);
		printk("mm_management : pagenum:%lx, phy addr:%px\n", p_frame->physical_page_frame_num, p_frame->physical_start_addr);
	}
	
	printk("mm_management : Free Pages Kernel Space--\n");
	list_for_each(pos, &mem->kernel_free_pages_list)
	{
		p_frame = list_entry(pos, struct page_frame, mm_memory_list);
		printk("mm_management : pagenum:%lx, phy addr:%px\n", p_frame->physical_page_frame_num, p_frame->physical_start_addr);
	}
}

static void check_alloc_pages(void)
{
	struct list_head *pos;
	struct page_frame *p_frame;
	
	printk("mm_management : Allocated User Space Pages--\n");
	list_for_each(pos, &mem->user_space_alloc_pages_list)
	{
		p_frame = list_entry(pos, struct page_frame, mm_memory_list);
		printk("mm_management : pagenum:%lx, phy addr:%px\n", p_frame->physical_page_frame_num, p_frame->physical_start_addr);
	}
	
	printk("mm_management : Allocated Kernel Space Pages--\n");
	list_for_each(pos, &mem->kernel_alloc_pages_list)
	{
		p_frame = list_entry(pos, struct page_frame, mm_memory_list);
		printk("mm_management : pagenum:%lx, phy addr:%px\n", p_frame->physical_page_frame_num, p_frame->physical_start_addr);
	}
}

static struct page_frame * mm_get_free_page_internal(bool get_kernel_page_flag)
{
	struct page_frame * p_frame;
	
	if(get_kernel_page_flag)
	{
		mutex_lock(&mem->mm_memory_mutex);
		if(list_empty(&mem->kernel_free_pages_list))
		{
			mutex_unlock(&mem->mm_memory_mutex);
			return NULL;
		}
		
		p_frame = list_first_entry(&mem->kernel_free_pages_list, struct page_frame, mm_memory_list);
		list_move_tail(&p_frame->mm_memory_list, &mem->kernel_alloc_pages_list);
		
		mem->num_free_kernel_pages -= 1;
		p_frame->free = false;
	}
	else
	{
		mutex_lock(&mem->mm_memory_mutex);
		if(list_empty(&mem->user_space_free_pages_list))
		{
			mutex_unlock(&mem->mm_memory_mutex);
			return NULL;
		}
		
		p_frame = list_first_entry(&mem->user_space_free_pages_list, struct page_frame, mm_memory_list);
		list_move_tail(&p_frame->mm_memory_list, &mem->user_space_alloc_pages_list);
		
		mem->num_free_user_space_pages -= 1;
		p_frame->free = false;
	}
	
	mutex_unlock(&mem->mm_memory_mutex);
	
	return (p_frame);
}

static inline uintptr_t set_PTE(uintptr_t p_frame_num)
{
	p_frame_num = p_frame_num & 0x0000000FFFFFFFFF; // Extracting 36bit page frame addr
	return p_frame_num | 0x0000003000000000; // Setting reference and validity bit
}


static uintptr_t mm_update_page_tables(uintptr_t virtual_p_frame_addr, int level, uintptr_t addr, uintptr_t physical_p_frame_num)
{
	struct page_frame * p_frame;
	
	uintptr_t virtual_p_frame_num = (virtual_p_frame_addr & 0x0000FFFFFFFFF000) >> 12; // 36bits pframe num
	//See if you can add above line of code in prev function to minimize computation
	
	uintptr_t ind;
	
	switch(level)
	{
		case 1: ind = (virtual_p_frame_num & 0xFF8000000) >> 27;
			break;
		
		case 2: ind = (virtual_p_frame_num & 0x007FC0000) >> 18;
			break;
			
		case 3: ind = (virtual_p_frame_num & 0x00003FE00) >> 9;
			break;
			
		case 4: ind = (virtual_p_frame_num & 0x0000001FF);
			break;
			
		default: return (uintptr_t)NULL;
	}
	
	printk("mm_management : ind=%lx\n", ind);
	
	uintptr_t * address = (uintptr_t *)(addr + ind*4);
	
	if( !( *address & 0x0000001000000000 ) )
	{
		if(level == 1 || level == 2 || level == 3)
		{
			p_frame = mm_get_free_page_internal(1);
			
			// Need to add page fault handler if we dont have enough page frames in the memory
			
			*address = set_PTE(p_frame->physical_page_frame_num);
		}
		else
		{
			*address = set_PTE(physical_p_frame_num);
		}
	}
	
	return (((uintptr_t)memory_addr >> 48) << 48) | ((*address & 0x0000000FFFFFFFFF) << 12);
}

static uintptr_t mm_update_page_table(uintptr_t physical_p_frame_num)
{
	uintptr_t addr;
	
	//mm_update_page_map_table(latest_virtual_p_frame, physical_p_frame_num);
	
	printk("mm_management : Virtual Page Address:%lx\n", latest_virtual_p_frame_addr_usr_space);
	printk("mm_management : Physical Page Address:%lx\n", physical_p_frame_num);
	
	printk("mm_management : Page map table Page Address:%lx\n", (uintptr_t)mem->cr3_page_table_addr);
	
	addr = mm_update_page_tables(latest_virtual_p_frame_addr_usr_space, 1, (uintptr_t)mem->cr3_page_table_addr, physical_p_frame_num);
	printk("mm_management : Page directory pointer table Page Address:%lx\n", addr);
	
	addr = mm_update_page_tables(latest_virtual_p_frame_addr_usr_space, 2, addr, physical_p_frame_num);
	printk("mm_management : Page directory table Page Address:%lx\n", addr);
	
	addr = mm_update_page_tables(latest_virtual_p_frame_addr_usr_space, 3, addr, physical_p_frame_num);
	printk("mm_management : Page table Page Address:%lx\n", addr);
	
	addr = mm_update_page_tables(latest_virtual_p_frame_addr_usr_space, 4, addr, physical_p_frame_num);
	printk("mm_management : Page Address:%lx\n", addr);
	
	return latest_virtual_p_frame_addr_usr_space;
	
}

static uintptr_t mm_get_free_page(void)
{
	struct page_frame * pframe = mm_get_free_page_internal(0);
	if(!pframe)
	{
		printk(KERN_ERR "mm_management : No Free Pages available in the memory\n");
		return 0;
	}
	
	uintptr_t virtual_p_frame_addr = mm_update_page_table(pframe->physical_page_frame_num);
	latest_virtual_p_frame_addr_usr_space += (0x0000000000001000);
	return virtual_p_frame_addr;
}

static uintptr_t mm_get_page_table(uintptr_t vfn, int level, uintptr_t addr)
{
	uintptr_t ind;
	
	switch(level)
	{
		case 1: ind = (vfn & 0xFF8000000) >> 27;
			break;
		
		case 2: ind = (vfn & 0x007FC0000) >> 18;
			break;
			
		case 3: ind = (vfn & 0x00003FE00) >> 9;
			break;
			
		case 4: ind = (vfn & 0x0000001FF);
			break;
			
		default: return (uintptr_t)NULL;
	}
	
	printk("mm_management : ind=%lx\n", ind);
	
	uintptr_t * address = (uintptr_t *)(addr + ind*4);
	
	return (((uintptr_t)memory_addr >> 48) << 48) | ((*address & 0x0000000FFFFFFFFF) << 12);
}

static uintptr_t mm_virtual_to_physical_address_translation(uintptr_t virtual_address)
{
	printk("mm_management : -------------\nmm_management : Address Tanslation:\nmm_management : Virtual Address : %lx\n", virtual_address);
	
	uintptr_t vfn = (virtual_address & 0x0000FFFFFFFFF000) >> 12;
	uintptr_t offset = virtual_address & 0x0FFF;
	
	uintptr_t addr;
	
	addr = mm_get_page_table(vfn, 1, (uintptr_t)mem->cr3_page_table_addr);
	addr = mm_get_page_table(vfn, 2, addr);
	addr = mm_get_page_table(vfn, 3, addr);
	addr = mm_get_page_table(vfn, 4, addr);
	
	printk("nmm_management :  Physical aaddress : %lx\n", addr | offset);
	return addr;
}

static int mm_free_page_internal(uintptr_t page_physical_address)
{
	struct page_frame * p_frame, * temp_p_frame;
	uintptr_t page_num = page_physical_address >> 12;
	
	mutex_lock(&mem->mm_memory_mutex);
	
	if(list_empty(&mem->user_space_alloc_pages_list))
	{
			mutex_unlock(&mem->mm_memory_mutex);
			return -PAGE_NOT_AVAILABLE;
	}
		
	list_for_each_entry_safe(p_frame, temp_p_frame, &mem->user_space_alloc_pages_list, mm_memory_list)
	{
			if(p_frame->physical_page_frame_num == page_num)
			{
				list_move_tail(&p_frame->mm_memory_list, &mem->user_space_free_pages_list);
				p_frame->free = true;
				mutex_unlock(&mem->mm_memory_mutex);
				return 0;
			}
	}
	
	if(list_empty(&mem->kernel_alloc_pages_list))
	{
			mutex_unlock(&mem->mm_memory_mutex);
			return -PAGE_NOT_AVAILABLE;
	}
		
	list_for_each_entry_safe(p_frame, temp_p_frame, &mem->kernel_alloc_pages_list, mm_memory_list)
	{
			if(p_frame->physical_page_frame_num == page_num)
			{
				list_move_tail(&p_frame->mm_memory_list, &mem->kernel_free_pages_list);
				p_frame->free = true;
				mutex_unlock(&mem->mm_memory_mutex);
				return 0;
			}
	}
	
	mutex_unlock(&mem->mm_memory_mutex);
	
	return -PAGE_NOT_AVAILABLE;
}

static void mm_free_page(uintptr_t page_virtual_address)
{
	uintptr_t page_physical_address;
	int err;
	
	page_physical_address = mm_virtual_to_physical_address_translation(page_virtual_address);
	
	if((err = mm_free_page_internal(page_physical_address)) != 0 )
	{
		printk(KERN_ERR "mm_management : Address not available in allocated list\n");
	}
}



static int __init mm_management_init(void)
{
	
	printk("mm_management : \nmm_management : Start ---------------------\n");
	initialize_memory();
	
	check_free_memory();
	check_alloc_pages();
	
	printk("mm_management : ---------------------\n");
	uintptr_t addr1 = mm_get_free_page();
	mm_free_page(addr1);
	
	printk("mm_management : ---------------------\n");
	check_free_memory();
	check_alloc_pages();
	
	printk("mm_management : ---------------------\n");
	uintptr_t addr2 = mm_get_free_page();
	mm_free_page(addr2);
	
	printk("mm_management : ---------------------\n");
	check_free_memory();
	check_alloc_pages();
	
	printk("mm_management : ---------------------\n");
	uintptr_t addr3 = mm_get_free_page();
	mm_free_page(addr3);
	
	printk("mm_management : ---------------------\n");
	check_free_memory();
	check_alloc_pages();
	
	latest_virtual_p_frame_addr_usr_space = 0x0000000000200000;
	
	printk("mm_management : ---------------------\n");
	uintptr_t addr4 = mm_get_free_page();
	mm_free_page(addr4);
	
	printk("mm_management : ---------------------\n");
	check_free_memory();
	check_alloc_pages();
	
	latest_virtual_p_frame_addr_usr_space = 0x0000000040000000;
	
	printk("mm_management : ---------------------\n");
	uintptr_t addr5 = mm_get_free_page();
	mm_free_page(addr5);
	
	printk("mm_management : ---------------------\n");
	check_free_memory();
	check_alloc_pages();
	
	latest_virtual_p_frame_addr_usr_space = 0x0000008000000000;
	
	printk("mm_management : ---------------------\n");
	uintptr_t addr6 = mm_get_free_page();
	mm_free_page(addr6);
	
	printk("mm_management : ---------------------\n");
	check_free_memory();
	check_alloc_pages();
	
	return 0;
}

static void __exit mm_management_exit(void)
{
	kfree(memory_addr);
	kfree(mem);
	printk("mm_management : mm_management_exit\n");
}

module_init(mm_management_init);
module_exit(mm_management_exit);

