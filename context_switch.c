/**
 * Date: 5/4/25
 * File: context_switch.c
 * Description: Context switching implementation for Yalnix OS
 */

#include "context_switch.h"

#include <hardware.h>
#include <stdbool.h>

#include "kernel.h"
#include "memory.h"
#include "pcb.h"

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    if (curr_pcb_p != NULL) {
        pcb_t *curr_proc = (pcb_t *)curr_pcb_p;
        pcb_t *next_proc = (pcb_t *)next_pcb_p;

        TracePrintf(3, "KCSwitch: From PID %d to PID %d.\n", curr_proc ? curr_proc->pid : -1, next_proc->pid);

        // Copy the current KernelContext (kc_in) into the old PCB
        memcpy(&curr_proc->kernel_context, kc_in, sizeof(KernelContext));

        // Set the next process
        current_process = next_proc;
        next_proc->state = PROCESS_RUNNING;

        // Update the global current_process variable
        map_kernel_stack(next_proc->kernel_stack);

        // Change Region 1 Page Table Base Register (REG_PTBR1) to the new PCB's Region 1 page table
        WriteRegister(REG_PTBR1, (unsigned long)next_proc->region1_pt);

        // Flush the TLB to ensure new mappings are used
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);  // Flush all TLB entries for safety

        // Return a pointer to the KernelContext in the new PCB
        TracePrintf(0, "Returning from KCSwitch\n");
        return &next_proc->kernel_context;
    } else {
        pcb_t *next_proc = (pcb_t *)next_pcb_p;

        // Set the next process
        current_process = next_proc;
        next_proc->state = PROCESS_RUNNING;

        // Update the global current_process variable
        map_kernel_stack(next_proc->kernel_stack);

        // Change Region 1 Page Table Base Register (REG_PTBR1) to the new PCB's Region 1 page table
        WriteRegister(REG_PTBR1, (unsigned long)next_proc->region1_pt);

        // Flush the TLB to ensure new mappings are used
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);  // Flush all TLB entries for safety

        TracePrintf(0, "Returning from KCSwitch\n");
        return &next_proc->kernel_context;
    }
}

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *na) {
    pcb_t *new_proc = (pcb_t *)new_pcb_p;
    TracePrintf(1, "KCCopy: Setting up kernel context for PID %d.\n", new_proc->pid);

    // Save the incoming KernelContext (kc_in) into the new PCB's kernel_context field.
    // This kc_in contains the state of the caller function just before KernelContextSwitch was invoked.
    memcpy(&new_proc->kernel_context, kc_in, sizeof(KernelContext));

    int num_kernel_stack_pages = KERNEL_STACK_MAXSIZE / PAGESIZE;
    char *current_kernel_stack_base = (char *)KERNEL_STACK_BASE;

    for (int i = 0; i < num_kernel_stack_pages; i++) {
        // Get the physical frame number (PFN) for the corresponding page in the new kernel stack's page table.
        pte_t *new_stack_pte = new_proc->kernel_stack + i;  // Assuming contiguous PTEs for kernel stack in new_proc->kernel_stack
        int dest_pfn = new_stack_pte->pfn;

        // Temporarily map the destination frame into Region 0 for copying.
        setup_temp_mapping(dest_pfn);

        // Copy contents of the current kernel stack page to the temporarily mapped destination
        memcpy((void *)TEMP_MAPPING_VADDR, (void *)((KSTACK_START_PAGE + i) << PAGESHIFT), PAGESIZE);

        // Remove the temporary mapping after copying
        remove_temp_mapping();
        new_stack_pte->valid = 1;
        new_stack_pte->prot = PROT_READ | PROT_WRITE;
    }

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);
    return kc_in;
}

void CopyPageTable(pcb_t *parent, pcb_t *child) {
    pte_t *parent_pt = parent->region1_pt;
    pte_t *child_pt = child->region1_pt;

    for (int i = 0; i < NUM_PAGES_REGION1; i++) {
        if (parent_pt[i].valid == 1) {
            int child_frame = allocate_frame();
            child_pt[i].pfn = child_frame;
            TracePrintf(0, "CopyPageTable: child process page table entry = %d, with physical frame number %d\n", i, child_frame);

            unsigned int parent_addr = (i + NUM_PAGES_REGION1) << PAGESHIFT;
            setup_temp_mapping(child_frame);  // Temporary map the frame to the scratch address
            memcpy((void *)TEMP_MAPPING_VADDR, (void *)parent_addr, PAGESIZE);
            remove_temp_mapping();

            child_pt[i].prot = parent_pt[i].prot;
            child_pt[i].valid = 1;
        }
    }
}

// void CopyPageTable(pcb_t *parent, pcb_t *child) {
//     pte_t *parent_pt = parent->region1_pt;
//     pte_t *child_pt = child->region1_pt;

//     for (int i = 0; i < NUM_PAGES_REGION1; i++) {
//         if (parent_pt[i].valid == 1) {
//             int child_frame = allocate_frame();
//             map_page(child_pt, i, child_frame, parent_pt[i].prot);
//             TracePrintf(0, "CopyPageTable: child process page table entry = %d, with physical frame number %d\n", i, child_frame);

//             unsigned int parent_addr = (i + NUM_PAGES_REGION1) << PAGESHIFT;
//             memcpy((void *)TEMP_MAPPING_VADDR, (void *)parent_addr, PAGESIZE);
//         }
//     }
// }

/**
 * @brief Sets up a temporary mapping in Region 0 for a given physical frame.
 * @param pfn The physical frame number to map.
 * @return The virtual address where the frame is mapped, or NULL on error.
 */
void setup_temp_mapping(int pfn) {
    TracePrintf(1, "setup_temp_mapping: Mapping PFN %d to temporary address %p.\n", pfn, TEMP_MAPPING_VADDR);
    int vpn = TEMP_MAPPING_VADDR >> PAGESHIFT;
    region0_pt[vpn].valid = 1;
    region0_pt[vpn].pfn = pfn;
    region0_pt[vpn].prot = PROT_READ | PROT_WRITE;
    WriteRegister(REG_TLB_FLUSH, TEMP_MAPPING_VADDR);
}

/**
 * @brief Removes a temporary mapping from Region 0.
 * @param addr The virtual address of the temporary mapping to remove.
 */
void remove_temp_mapping(void) {
    // Invalidate the PTE for the given virtual address 'addr' in region0_pt.
    TracePrintf(0, "remove_temp_mapping: removing mapping for virtual page number: %d\n", TEMP_MAPPING_VADDR >> PAGESHIFT);
    int vpn = TEMP_MAPPING_VADDR >> PAGESHIFT;
    region0_pt[vpn].valid = 0;
}

/**
 * @brief Maps the kernel stack of the current process (identified by its physical frames) into Region 0.
 *
 * This function is called during a context switch to ensure that the kernel
 * stack of the incoming process is correctly mapped into the fixed kernel
 * stack virtual address range in Region 0.
 *
 * @param kernel_stack_pt A pointer to the page table that describes the kernel stack.
 * @return 0 on success, ERROR on failure.
 */
int map_kernel_stack(pte_t *kernel_stack_pt) {
    TracePrintf(1, "map_kernel_stack: Re-mapping kernel stack in Region 0.\n");

    int num_pages = KERNEL_STACK_MAXSIZE >> PAGESHIFT;
    int page = (int)KERNEL_STACK_BASE >> PAGESHIFT;

    memcpy(&region0_pt[page], kernel_stack_pt, num_pages * sizeof(pte_t));

    //    for (int i = 0; i < num_kernel_stack_pages; i++) {
    //        // Get the PFN from the provided kernel_stack_pt
    //        pte_t *src_pte = kernel_stack_pt + i;
    //        int pfn = src_pte->pfn; // Get the physical frame number from the source PTE
    //
    //        // Map each physical frame of the new kernel stack to its corresponding
    //        TracePrintf(0, "map_kernel_stack: physical frame number is: %d\n", );
    //        map_page(region0_pt, (int)(kernel_stack_vaddr_start + i * PAGESIZE) >> PAGESHIFT, pfn, PROT_READ | PROT_WRITE);
    //    }

    TracePrintf(0, "Exit map_kernel_stack\n");

    return 0;
}
