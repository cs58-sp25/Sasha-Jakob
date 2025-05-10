/**
 * Date: 5/4/25
 * File: memory.c
 * Description: Memory management implementation for Yalnix OS
 */

#include "memory.h"
#include "hardware.h"

int vm_enabled = 0;       // Initialize to 0 (VM disabled) by default
void *kernel_brk = NULL;  // Current kernel break address
void *user_brk = NULL;    // Current user break address
int frame_bitMap[NUM_VPN] = {0};

int allocate_frame(void) {
    // If no free frames remain, return error (-1)
    // Search bitmap for first available frame (bit = 0)
    // Mark frame as used (set bit to 1)
    // Decrement num_free_frames counter
    // Return the physical frame number (pfn)
}


void free_frame(int pfn) {
    // Validate pfn is within range
    // Check if frame is currently marked as used
    // Mark frame as free (set bit to 0)
    // Increment num_free_frames counter
}

int SetKernelBrk(void *addr) {
    if (!vm_enabled) {
        // Before VM enabled: just track the kernel break
        if (addr > kernel_orig_brk && addr < KERNEL_STACK_BASE) {
            kernel_brk = addr;
            return 0;
        } else {
            return ERROR;
        }
    } else {
        // After VM enabled: need to actually allocate frames and update page tables
        void *old_brk = kernel_brk;
        void *new_brk = addr;
        
        // Round up to page boundary
        new_brk = (void *)UP_TO_PAGE((unsigned int)new_brk);
        
        // If decreasing break, just update the break pointer
        if (new_brk <= old_brk) {
            kernel_brk = new_brk;
            return 0;
        }
        
        // If increasing break, map new frames
        for (void *p = old_brk; p < new_brk; p += PAGESIZE) {
            int vpn = (int)p >> PAGESHIFT;
            int pfn = get_free_frame(); // Get a free physical frame
            
            if (pfn == ERROR) {
                return ERROR; // Out of physical memory
            }
            
            // Update the page table entry
            region0_pt[vpn].valid = 1;
            region0_pt[vpn].prot = PROT_READ | PROT_WRITE;
            region0_pt[vpn].pfn = pfn;
            
            // Flush the TLB for this page
            WriteRegister(REG_TLB_FLUSH, (unsigned int)p);
        }
        
        kernel_brk = new_brk;
        return 0;
    }
}



void init_page_table(int kernel_text_start, int kernel_data_start, int kernel_brk_start) {
    TracePrintf(0, "Initializing page table...\n");
    // Set the base address for the region0 page table in hardware
    unsigned int last_physical_address = MAX_PMEM_SIZE - 1; // Calculate the last physical address
    unsigned int page_table_size = MAX_PT_LEN * sizeof(struct pte); // Calculate size needed for the page table
    unsigned int p_address_region0_base = DOWN_TO_PAGE(last_physical_address - page_table_size); // Place page table at the end of physical memory

    // Initialize the kernel text, data, and heap sections in the page table
    for (int vpn = kernel_text_start; vpn < kernel_brk_start; vpn++) {  //
        plt_e *entry = (plt_e *)(p_address_region0_base + vpn * sizeof(plt_e));
        entry->valid = 1;
        if (vpn < kernel_data_start) { // Set protection bits appropriately
            entry->prot = PROT_READ | PROT_EXEC;  // Text section: read+execute
        } else {
            entry->prot = PROT_READ | PROT_WRITE;  // Data/heap: read+write
        }
        entry->pfn = vpn;
        frame_bitMap[vpn] = 1;  // Mark the frame as used in the bitmap
    }
    TracePrintf(0, "Kernel text, data, and heap initialized with %d total entries\n", kernel_brk_start);

    void * kernel_brk = UP_TO_PAGE(kernel_brk_start * PAGESIZE);  // Align to page boundary
    if (SetKernelBrk(kernel_brk) != 0) {
        TracePrintf(0, "ERROR: Failed to set kernel break to address %p\n", kernel_brk);
        return ERROR;  // Out of physical memory
    }

    // Initialize the kernel stack in the page table
    int base_kernelStack_page = KERNEL_STACK_BASE / PAGESIZE;  // Calculate the base page number for kernel stack
    int kernelStack_limit_page = KERNEL_STACK_LIMIT / PAGESIZE;  // Calculate the limit page number for kernel stack
    for (int vpn = base_kernelStack_page; vpn < kernelStack_limit_page; vpn ++) {
        plt_e *entry = (plt_e *)(p_address_region0_base + vpn * sizeof(plt_e));
        entry->valid = 1;
        entry->prot = entry->prot = PROT_READ | PROT_WRITE;  // Set read+write permissions for kernel stack
        entry->pfn = vpn;       // Set frame numbers to the ones proceeding kernel text, data, and heap frames
        frame_bitMap[vpn] = 1;  // Mark the frame as used in the bitmap
    }
    TracePrintf(0, "Kernel stack initialized with %d total entries\n", (KERNEL_STACK_LIMIT - KERNEL_STACK_BASE) / sizeof(plt_e));

    // Set page table registers to tell hardware where to find the page table
    int numPages_inRegion0 = VMEM_0_LIMIT / PAGESIZE;  // Number of pages in Region 0
    WriteRegister(REG_PTBR0, p_address_region0_base); // Set base address of region0 page table in hardware
    WriteRegister(REG_PTLR0, numPages_inRegion0); // Set limit of region0 page table in hardware
    TracePrintf(0, "Wrote region 0 page table base register to %d\n", p_address_region0_base);
    TracePrintf(0, "Wrote region 0 page table limit register to %d\n", numPages_inRegion0);

    // Set up the base physicall adress of region 1
    unsigned int p_address_region1_base = p_address_region0_base + numPages_inRegion0 * sizeof(plt_e); // Place region1 PT at the end of region0 PT
    int vpn = numPages_inRegion0 + 1; // Set the virtual page number for the first page of region 1
    plt_e *entry = (plt_e *)(p_address_region1_base);
    entry->valid = 1;
    entry->prot = entry->prot = PROT_READ | PROT_WRITE;  // Set read+write permissions for kernel stack
    entry->pfn = 1; // Set frame number to 1 since it is the first page of region 1
    frame_bitMap[vpn] = 1;  // Mark the frame as used in the bitmap
    TracePrintf(0, "Region 1 page table initialized with %d total entries\n", 1);

    WriteRegister(REG_PTBR1, p_address_region1_base); // Set base address of region1 page table in hardware
    WriteRegister(REG_PTLR1, 1); // Set limit of region1 page table in hardware (just 1 entry so far)
    TracePrintf(0, "Wrote region 1 page table base register to %d\n", p_address_region1_base);
    TracePrintf(0, "Wrote region 1 page table limit register to %d\n", 1);
}


void enable_virtual_memory(void) {
    // Write 1 to REG_VM_ENABLE register to enable virtual memory
    // This is a critical transition point - after this, all addresses are virtual
    WriteRegister(REG_VM_ENABLE, 1);

    // Update global flag to track that VM is now enabled
    // This flag helps kernel code know if physical or virtual addressing is in use
    vm_enabled = 1;
    TracePrintf(0, "Virtual memory enabled\n");
}


void map_page(struct pte *pt, int vpn, int pfn, int prot) {
    // Validate vpn and pfn are within valid ranges
    // Set physical frame number in page table entry
    // Set protection bits according to prot parameter
    // Set valid bit to 1
    // If vm_enabled, flush TLB entry for this vpn
}

void unmap_page(struct pte *pt, int vpn) {
    // Validate vpn is within valid range
    // Clear valid bit in page table entry
    // If vm_enabled, flush TLB entry for this vpn
    // Note: Does not free the physical frame
}

int copy_page_table(struct pte *src, struct pte *dest, int copy_frames) {
    // Loop through all entries in source page table
    // For each valid entry:
    //   If copy_frames is true:
    //     Allocate new physical frame
    //     If allocation fails, free all previously allocated frames and return ERROR
    //     Copy content from source frame to new frame
    //     Map vpn to new frame in dest page table with same protection
    //   Else:
    //     Map vpn to same frame in dest page table with same protection
    // Return 0 on success
}

int handle_memory_trap(UserContext *uctxt) {
    // Get fault address from uctxt
    // Calculate virtual page number (vpn) from fault address
    // Get current process's Region 1 page table

    // If fault is in stack growth region:
    //   Allocate new physical frame
    //   If allocation succeeds:
    //     Map new frame to the faulting vpn with appropriate protection
    //     Update process's stack limit if needed
    //     Return 0 (success)
    //   Else return ERROR (no memory)
    // Else return ERROR (invalid memory access)
}

int allocate_user_stack(struct pte *region1_pt, int num_pages) {
    // Calculate starting vpn for stack (at top of Region 1)
    // For each stack page (working downward from top):
    //   Allocate physical frame
    //   If allocation fails, free all previously allocated frames and return ERROR
    //   Map vpn to frame with PROT_READ | PROT_WRITE permissions
    // Return 0 on success
}

/**
 * Allocates and maps a kernel stack
 *
 * @param pcb Pointer to the PCB to store frame numbers (NULL for initial kernel stack)
 * @param map_now If true, map the stack to virtual memory immediately
 * @return 0 on success, ERROR on failure
 */
int allocate_kernel_stack(pcb_t *pcb, int map_now) {
    // Calculate number of pages needed for kernel stack
    int stack_pages = KERNEL_STACK_MAXSIZE / PAGESIZE;
    if (KERNEL_STACK_MAXSIZE % PAGESIZE != 0) {
        stack_pages++;  // Round up if not perfectly divisible by page size
    }

    // Calculate the base page number for kernel stack within Region 0
    int stack_base_page = (KERNEL_STACK_BASE - VMEM_0_BASE) >> PAGESHIFT;

    // Array to store allocated frame numbers
    int kernel_stack_frames[stack_pages];

    // Allocate physical frames for the kernel stack
    for (int i = 0; i < stack_pages; i++) {
        int frame = allocate_frame();
        if (frame == -1) {
            // Free any frames we've already allocated
            for (int j = 0; j < i; j++) {
                free_frame(kernel_stack_frames[j]);
            }
            TracePrintf(0, "ERROR: Failed to allocate frame for kernel stack\n");
            return ERROR;
        }
        kernel_stack_frames[i] = frame;

        // If this is for a process, store frame numbers in PCB
        if (pcb != NULL) {
            pcb->kernel_stack_frames[i] = frame;
        }
    }

    // If requested, map the stack frames to virtual memory now
    if (map_now) {
        pte_t *region0_pt = (pte_t *)ReadRegister(REG_PTBR0);

        // Map the frames to the kernel stack virtual address range
        for (int i = 0; i < stack_pages; i++) {
            int page_index = stack_base_page + i;
            region0_pt[page_index].valid = 1;
            region0_pt[page_index].prot = PROT_READ | PROT_WRITE;
            region0_pt[page_index].pfn = kernel_stack_frames[i];

            // Mark the frame as used in the frame map
            mark_frame_used(frame_map, kernel_stack_frames[i]);
        }

        // Leave one page unmapped as red zone below the kernel stack
        if (stack_base_page > 0) {
            region0_pt[stack_base_page - 1].valid = 0;
        }

        // Flush TLB entries for kernel stack to ensure updates are seen
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);
    }

    TracePrintf(3, "Kernel stack allocated with %d pages\n", stack_pages);
    return 0;
}

int map_kernel_stack(int *kernel_stack_pages) {
    // Calculate vpn starting at KERNEL_STACK_BASE
    // For each frame in kernel_stack_pages:
    //   Map vpn to frame with PROT_READ | PROT_WRITE permissions
    // Return 0 on success
}

struct pte *get_region0_pt(void) {
    // Return pointer to Region 0 page table
    return region0_pt;
}

void enable_virtual_memory(void) {
    // Write 1 to REG_VM_ENABLE register
    // Set vm_enabled flag to 1
    // After this call, all addresses are interpreted as virtual
}

int is_valid_user_pointer(void *addr, int len, int prot) {
    // Check if address is in user space (Region 1)
    // Calculate start and end virtual page numbers
    // For each page in range:
    //   Check if page is mapped in current process's Region 1 page table
    //   Check if page has requested protection bits
    // Return 1 if all checks pass, 0 otherwise
}

int is_valid_user_string(char *str, int max_len) {
    // Check if string pointer is in user space
    // Safely read characters until null terminator or max_len
    // Return string length (including null) if valid, -1 if invalid
}

int copy_from_user(void *dst, void *src, int len) {
    // Validate src pointer is in user space and readable
    // Validate length is reasonable
    // Copy data from user space to kernel space
    // Handle case where data spans multiple pages
    // Return 0 on success, ERROR on failure
}

int copy_to_user(void *dst, void *src, int len) {
    // Validate dst pointer is in user space and writable
    // Validate length is reasonable
    // Copy data from kernel space to user space
    // Handle case where data spans multiple pages
    // Return 0 on success, ERROR on failure
}

int get_physical_frame(void *addr, struct pte *pt) {
    // Calculate virtual page number from address
    // If pt is NULL, use current process's page table
    // Check if virtual page is mapped (valid bit set)
    // Return physical frame number if mapped, -1 otherwise
}

// TODO: create kernel_malloc, kernel_free, and perhaps memset
