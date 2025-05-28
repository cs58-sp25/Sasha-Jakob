/**
 * Date: 5/4/25
 * File: context_switch.c
 * Description: Context switching implementation for Yalnix OS
 */

#include "context_switch.h"
#include <hardware.h>
#include "kernel.h"
#include "pcb.h"
#include "memory.h"

#define TEMP_MAPPING_VADDR (void *)(KERNEL_STACK_BASE - PAGESIZE)


KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    pcb_t *curr_proc = (pcb_t *)curr_pcb_p;
    pcb_t *next_proc = (pcb_t *)next_pcb_p;

    TracePrintf(3, "KCSwitch: From PID %d to PID %d.\n", curr_proc ? curr_proc->pid : -1, next_proc->pid);

    // Copy the current KernelContext (kc_in) into the old PCB [cite: 457]
    if (curr_proc) {
        curr_proc->kernel_context = kc_in;
    }

    // Update the global current_process variable
    current_process = next_proc;

    // Change the Region 0 kernel stack mappings to those for the new PCB [cite: 457]
    map_kernel_stack(next_proc->kernel_stack_pages);

    // Change Region 1 Page Table Base Register (REG_PTBR1) to the new PCB's Region 1 page table
    WriteRegister(REG_PTBR1, (unsigned long)next_proc->region1_pt);
    // Also update REG_PTLR1 if your Region 1 page table size can vary, though for now assuming fixed size.

    // Flush the TLB to ensure new mappings are used [cite: 457]
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL); // Flush all TLB entries for safety

    // Return a pointer to the KernelContext in the new PCB [cite: 457]
    return next_proc->kernel_context;
}


KernelContext *KCCopy(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    // kc_in: KernelContext of the calling context (e.g., KernelStart or a parent process for Fork)
    // curr_pcb_p: Pointer to the current PCB (NULL for initial KernelStart call)
    // next_pcb_p: Pointer to the new PCB (initPCB in this case)

    pcb_t *new_proc = (pcb_t *)next_pcb_p;
    TracePrintf(1, "KCCopy: Setting up kernel context for PID %d.\n", new_proc->pid);

    // Save the incoming KernelContext (kc_in) into the new PCB's kernel_context field.
    // This kc_in contains the state of the caller function just before KernelContextSwitch was invoked.
    new_proc->kernel_context = kc_in;

    // Copy the current kernel stack contents into the new kernel stack frames. [cite: 457]
    // The current kernel stack is in Region 0, mapped from KERNEL_STACK_BASE.
    // The new kernel stack frames are physical frames stored in new_proc->kernel_stack_pages.

    int num_kernel_stack_pages = KERNEL_STACK_MAXSIZE / PAGESIZE;
    char *current_kernel_stack_base = (char *)KERNEL_STACK_BASE;

    for (int i = 0; i < num_kernel_stack_pages; i++) {
        // Temporarily map the destination frame into Region 0 for copying. [cite: 457]
        // `setup_temp_mapping` should map the physical frame `new_proc->kernel_stack_pages[i]`
        // to a temporary virtual address in Region 0 (e.g., KERNEL_STACK_BASE - PAGESIZE).
        void *mapped_dest_addr = setup_temp_mapping(new_proc->kernel_stack_pages[i]);
        if (mapped_dest_addr == NULL) {
            TracePrintf(0, "ERROR: KCCopy: Failed to set up temporary mapping for frame %d.\n", new_proc->kernel_stack_pages[i]);
            // Return NULL to indicate failure to KernelContextSwitch
            return NULL;
        }

        // Copy contents of the current kernel stack page to the temporarily mapped destination
        memcpy(mapped_dest_addr, current_kernel_stack_base + (i * PAGESIZE), PAGESIZE);

        // Remove the temporary mapping after copying
        remove_temp_mapping(mapped_dest_addr);
    }

    // Return the new process's kernel context for the switch.
    // Since KCCopy is called via KernelContextSwitch, returning this will cause the hardware
    // to load this kernel context.
    return new_proc->kernel_context;
}


/**
 * @brief Sets up a temporary mapping in Region 0 for a given physical frame.
 * @param pfn The physical frame number to map.
 * @return The virtual address where the frame is mapped, or NULL on error.
 */
void *setup_temp_mapping(int pfn) {
    TracePrintf(1, "setup_temp_mapping: Mapping PFN %d to temporary address %p.\n", pfn, TEMP_MAPPING_VADDR);
    // In a real implementation, you would:
    // 1. Ensure TEMP_MAPPING_VADDR is a safe, dedicated virtual page in Region 0.
    // 2. Map this virtual page to the given physical frame 'pfn' in region0_pt.
    // 3. Flush the TLB entry for TEMP_MAPPING_VADDR to ensure the new mapping is visible.

    // Placeholder: Assuming map_page can handle this
    map_page(region0_pt, DOWN_TO_PAGE(TEMP_MAPPING_VADDR), pfn, PROT_READ | PROT_WRITE);
    WriteRegister(REG_TLB_FLUSH, (unsigned long)TEMP_MAPPING_VADDR); // Flush specific TLB entry

    return TEMP_MAPPING_VADDR;
}

/**
 * @brief Removes a temporary mapping from Region 0.
 * @param addr The virtual address of the temporary mapping to remove.
 */
void remove_temp_mapping(void *addr) {
    TracePrintf(1, "remove_temp_mapping: Unmapping address %p.\n", addr);
    // In a real implementation, you would:
    // 1. Invalidate the PTE for the given virtual address 'addr' in region0_pt.
    // 2. Flush the TLB entry for that virtual page.

    // Placeholder: Assuming unmap_page is available
    // unmap_page(region0_pt, VPN_FROM_ADDR(addr)); // You would need to implement unmap_page if not available
    pte_t *entry = region0_pt + DOWN_TO_PAGE(addr);
    entry->valid = 0; // Invalidate the PTE
    WriteRegister(REG_TLB_FLUSH, (unsigned long)addr); // Flush specific TLB entry
}

/**
 * @brief Maps the kernel stack of the current process (identified by its physical frames) into Region 0.
 *
 * This function is called during a context switch to ensure that the kernel
 * stack of the incoming process is correctly mapped into the fixed kernel
 * stack virtual address range in Region 0.
 *
 * @param kernel_stack_frames An array of physical frame numbers that constitute
 * the kernel stack of the process.
 * @return 0 on success, ERROR on failure.
 */
int map_kernel_stack(int *kernel_stack_frames) {
    TracePrintf(1, "map_kernel_stack: Re-mapping kernel stack in Region 0.\n");

    int num_kernel_stack_pages = KERNEL_STACK_MAXSIZE >> PAGESHIFT;
    void *kernel_stack_vaddr_start = (void *)KERNEL_STACK_BASE;

    for (int i = 0; i < num_kernel_stack_pages; i++) {
        // Map each physical frame of the new kernel stack to its corresponding
        // virtual page in Region 0.
        // Permissions: PROT_READ | PROT_WRITE for kernel stack.
        map_page(region0_pt, DOWN_TO_PAGE(kernel_stack_vaddr_start + i * PAGESIZE),
                 kernel_stack_frames[i], PROT_READ | PROT_WRITE);
    }
    // A full TLB flush (TLB_FLUSH_ALL) typically happens in KCSwitch itself after all mappings are done.
    // But specific flush for kernel stack region might be needed if not doing a full flush.
    // WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0); // Flush Region 0 TLB entries related to kernel stack

    return 0;
}
