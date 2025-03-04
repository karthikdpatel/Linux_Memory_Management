#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include "include/mm_swap_space.h"

MODULE_LICENSE("Dual BSD/GPL");

struct mm_physical_memory * mem;

static int initialise_simulator(void)
{
	int err;
	
	if((err = initialize_memory(mem)) != 0)
	{
		return err;
	}
	
	if((err = initialize_pframes(mem)) != 0)
	{
		return err;
	}
	
	initialise_swap_space();
	
	return 0;
}

static int __init mm_simulator_init(void)
{
	
	printk("mm_management : \nmm_management : Start ---------------------\n");
	
	if(initialise_simulator())
	{
		return -1;
	}
	//print_list();
	
	uintptr_t addr1, p_addr1;
	printk("mm_management : ---------------------------\n");
	int err = get_free_page(mem, &addr1);
	
	if(!err)
	{
		printk("mm_management : addr1:%lx\n", addr1);
	}
	
	virtual_to_physical_address(mem, addr1, &p_addr1);
	printk("mm_management : physical address adr1:%lx\n", p_addr1);
	
	err = mm_free_page(mem, addr1);
	
	//print_list();
	
	return 0;
}


static void __exit mm_simulator_exit(void)
{
	//uninitialize_memory();
	printk("mm_management : mm_management_exit\n");
}

module_init(mm_simulator_init);
module_exit(mm_simulator_exit);

