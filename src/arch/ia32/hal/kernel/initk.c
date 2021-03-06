#include "common/include/data.h"
#include "common/include/memory.h"
#include "common/include/memlayout.h"
#include "hal/include/print.h"
#include "hal/include/mem.h"
#include "hal/include/lib.h"
#include "hal/include/cpu.h"
#include "hal/include/task.h"
#include "hal/include/int.h"
#include "hal/include/kernel.h"
#include "hal/include/time.h"
#include "hal/include/syscall.h"

#ifndef __HAL__
#define __HAL__
#endif
#include "kernel/include/hal.h"


static struct hal_exports *hexp;
static void asmlinkage (*kernel_entry)(struct hal_exports *exp);

struct kernel_exports *kernel;


void init_kernel()
{
    kprintf("Initializing kernel\n");
    
    /*
     * Build HAL exports
     */
    hexp = (struct hal_exports *)kalloc(sizeof(struct hal_exports));
    
    // Kernel exprts
    kernel = (struct kernel_exports *)kalloc(sizeof(struct kernel_exports));
    hexp->kernel = kernel;
    
    // General functions
    hexp->kprintf = kprintf;
    hexp->time = get_system_time;
    hexp->halt = wrap_halt;
    
    // Kernel info
    hexp->kernel_page_dir_pfn = KERNEL_PDE_PFN;
    
    // Core image info
    hexp->coreimg_load_addr = COREIMG_LOAD_PADDR;
    
    // MP
    hexp->num_cpus = num_cpus;
    hexp->get_cur_cpu_id = wrap_get_cur_cpu_id;
    
    // Physical memory info
    hexp->free_mem_start_addr = PFN_TO_ADDR(get_bootparam()->free_pfn_start);
    hexp->paddr_space_end = paddr_space_end;
    hexp->get_next_mem_zone = get_next_mem_zone;
    
    // IO Ports
    hexp->io_in = wrap_io_in;
    hexp->io_out = wrap_io_out;
    
    // Interrupt
    hexp->disable_local_interrupt = disable_local_int;
    hexp->enable_local_interrupt = enable_local_int;
    hexp->restore_local_interrupt = restore_local_int;
    
    // Mapping
    hexp->map_user = wrap_user_map;
    hexp->unmap_user = wrap_user_unmap;
    hexp->get_paddr = wrap_get_paddr;
    
    // Load image
    hexp->load_exe = wrap_load_exe;
    
    // Address space
    hexp->vaddr_space_end = USER_VADDR_SPACE_END;
    hexp->init_addr_space = wrap_init_addr_space;
    
    // Context
    hexp->init_context = init_thread_context;
    hexp->set_context_param = set_thread_context_param;
    hexp->switch_context = switch_context;
    hexp->set_syscall_return = set_syscall_return;
    hexp->sleep = wrap_sleep;
    hexp->yield = wrap_yield;
    
    // TLB
    hexp->invalidate_tlb = wrap_invalidate_tlb;
    
    /*
     * Call kernel's entry
     */
    kprintf("\tKernel entry: %p\n", get_bootparam()->kernel_entry_addr);
    kernel_entry = (void *)get_bootparam()->kernel_entry_addr;
    kernel_entry(hexp);
    
    kprintf("Kernel has been initialized!\n");
}

void kernel_dispatch(struct kernel_dispatch_info *kdi)
{
    // Save old CR3
    ulong cr3 = 0;
    __asm__ __volatile__
    (
        "movl   %%cr3, %%ebx;"
        : "=b" (cr3)
        :
    );
    
    // Switch to kernel AS
    __asm__ __volatile__
    (
        "movl   %%eax, %%cr3;"
        "jmp    _cr3_switched;"
        "_cr3_switched:"
        "nop;"
        :
        : "a" (KERNEL_PDE_PFN << 12)
    );
    
    // Then call kernel dispatcher
    kernel->dispatch(*get_per_cpu(ulong, cur_running_sched_id), kdi);
    
    // Switch back to user AS
    __asm__ __volatile__
    (
        "movl   %%eax, %%cr3;"
        "jmp    _cr3_switched_to_user;"
        "_cr3_switched_to_user:"
        "nop;"
        :
        : "a" (cr3)
    );
}
