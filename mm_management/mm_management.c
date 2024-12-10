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

static int mm_free_page_internal(uintptr_t page_num)
{
	struct page_frame * p_frame, * temp_p_frame;
	
	//spin_lock_irqsave(&mem->mm_memory_spinlock, mem->mm_memory_spinlock_irq_flag);
	mutex_lock(&mem->mm_memory_mutex);
	
	if(list_empty(&mem->alloc_pages_list))
	{
		//spin_unlock_irqrestore(&mem->mm_memory_spinlock, mem->mm_memory_spinlock_irq_flag);
		mutex_unlock(&mem->mm_memory_mutex);
		return -PAGE_NOT_AVAILABLE;
	}
	
	list_for_each_entry_safe(p_frame, temp_p_frame, &mem->alloc_pages_list, list)
	{
		if(p_frame->physical_page_frame_num == page_num)
		{
			list_move_tail(&p_frame->list, &mem->free_pages_list);
			mutex_unlock(&mem->mm_memory_mutex);
			return 0;
		}
	}
	
	//spin_unlock_irqrestore(&mem->mm_memory_spinlock, mem->mm_memory_spinlock_irq_flag);
	mutex_unlock(&mem->mm_memory_mutex);
	
	return -PAGE_NOT_AVAILABLE;
}

static void mm_free_page(uintptr_t page_num)
{
	if(mm_free_page_internal(page_num))
	{
		printk(KERN_ERR "mm_management : Address not available in allocated list\n");
	}
}

static struct page_frame * mm_get_free_page_internal(void)
{
	struct page_frame * p_frame;
	//spin_lock_irqsave(&mem->mm_memory_spinlock, mem->mm_memory_spinlock_irq_flag);
	mutex_lock(&mem->mm_memory_mutex);
	
	if(list_empty(&mem->free_pages_list))
	{
		//spin_unlock_irqrestore(&mem->mm_memory_spinlock, mem->mm_memory_spinlock_irq_flag);
		mutex_unlock(&mem->mm_memory_mutex);
		return NULL;
	}
	
	p_frame = list_first_entry(&mem->free_pages_list, struct page_frame, list);
	list_move_tail(&p_frame->list, &mem->alloc_pages_list);
	
	mem->num_free_pages -= 1;
	
	//spin_unlock_irqrestore(&mem->mm_memory_spinlock, mem->mm_memory_spinlock_irq_flag);
	mutex_unlock(&mem->mm_memory_mutex);
	
	return (p_frame);
}

static uintptr_t mm_get_free_page(void)
{
	struct page_frame * pframe = mm_get_free_page_internal();
	if(!pframe)
	{
		printk(KERN_ERR "mm_management : No Free Pages available in the memory\n");
		return 0;
	}
	return pframe->physical_page_frame_num;
}


static void check_free_memory(void)
{
	struct list_head *pos;
	struct page_frame *p_frame;
	
	printk("mm_management : Free Pages--\n");
	list_for_each(pos, &mem->free_pages_list)
	{
		p_frame = list_entry(pos, struct page_frame, list);
		printk("mm_management : pagenum:%lx, phy addr:%px\n", p_frame->physical_page_frame_num, p_frame->physical_start_addr);
	}
	
}

static void check_alloc_pages(void)
{
	struct list_head *pos;
	struct page_frame *p_frame;
	
	printk("mm_management : Allocated Pages--\n");
	list_for_each(pos, &mem->alloc_pages_list)
	{
		p_frame = list_entry(pos, struct page_frame, list);
		printk("mm_management : pagenum:%lx, phy addr:%px\n", p_frame->physical_page_frame_num, p_frame->physical_start_addr);
	}
}


static int initialize_memory(void)
{
	memory_addr = kmalloc(TOTAL_MEMORY_EXP, GFP_KERNEL);
	if(!memory_addr)
	{
		printk(KERN_ERR "mm_management : Error in getting the total memory\n");
		return -1;
	}
	
	
	/*printk("mm_management : start addr:%px\n", memory_addr);
	
	void * shifted_addr = (void *)((uintptr_t)memory_addr >> 12);
	
	printk("mm_management : shifted addr:%px\n", shifted_addr);
	
	uintptr_t mem = ((uintptr_t)memory_addr >> 12);
	
	printk("mm_management : mem addr:%lx\n", mem);*/
	
	mem = kmalloc(sizeof(struct mm_memory), GFP_KERNEL);
	if(!mem)
	{
		printk(KERN_ERR "mm_management : Error in allocating memory structure\n");
		return -1;
	}
	
	mem->num_total_pages = TOTAL_MEMORY_EXP / PAGE_SIZE_EXP;
	mem->num_free_pages = mem->num_total_pages;
	
	INIT_LIST_HEAD(&mem->free_pages_list);
	INIT_LIST_HEAD(&mem->alloc_pages_list);
	
	mutex_init(&mem->mm_memory_mutex);
	
	int i;
	struct page_frame *p_frame;
	
	for(i=0;i<mem->num_total_pages;i++)
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
		p_frame->validity = false;
		p_frame->modified = false;
		list_add_tail(&p_frame->list, &mem->free_pages_list);
	}
	
	return 0;
}

static int __init mm_management_init(void)
{
	
	printk("mm_management : Start ---------------------\n");
	initialize_memory();
	
	check_free_memory();
	check_alloc_pages();
	
	uintptr_t page_num1 = mm_get_free_page();
	uintptr_t page_num2 = mm_get_free_page();
	uintptr_t page_num3 = mm_get_free_page();
	uintptr_t page_num4 = mm_get_free_page();
	uintptr_t page_num5 = mm_get_free_page();
	uintptr_t page_num6 = mm_get_free_page();
	
	printk("--------------------\n");
	
	check_free_memory();
	check_alloc_pages();
	
	mm_free_page(page_num1+1);
	mm_free_page(page_num1);
	mm_free_page(page_num2);
	mm_free_page(page_num3);
	mm_free_page(page_num4);
	mm_free_page(page_num5);
	//mm_free_page(page_num6);
	
	mm_free_page(page_num5);
	
	printk("--------------------\n");
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

