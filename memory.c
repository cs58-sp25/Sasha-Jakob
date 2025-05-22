/**
 * Date: 5/4/25
 * File: memory.c
 * Description: Memory management implementation for Yalnix OS
 */

#include "memory.h"
#include <hardware.h>
#include <yalnix.h>

#define MIN_FRAME_ALLOWED 30

int vm_enabled = 0;       // Initialize to 0 (VM disabled) by default
void *kernel_brk = NULL;  // Current kernel break address
void *user_brk = NULL;    // Current user break address
int *frame_bitMap;      // Bitmap to track free/used frames
pte_t region0_pt[MAX_PT_LEN]; // Page table for Region 0
pte_t region1_pt[MAX_PT_LEN]; // Page table for Region 1


int allocate_frame(void) {
    TracePrintf(1, "Enter allocate_frame()\n");
    // Iterate through the frame_bitMap to find a free frame
    for (int i = 0; i < NUM_VPN; i++) { // Assuming NUM_VPN is the total number of physical frames
        if (frame_bitMap[i] == 0) { // If the frame is free (0 means free, 1 means used)
            frame_bitMap[i] = 1;    // Mark it as used
            TracePrintf(1, "allocate_frame: Allocated frame %d\n", i);
            return i;               // Return the physical frame number
        }
    }
    TracePrintf(0, "allocate_frame: ERROR: No free physical frames available\n");
    return ERROR; // No free frames
}


void free_frame(int pfn) {
    TracePrintf(1, "Enter free_frame(%d)\n", pfn);
    // Basic validation for the physical frame number
    if (pfn < 0 || pfn >= NUM_VPN) { // Assuming NUM_VPN is the total number of physical frames
        TracePrintf(0, "free_frame: ERROR: Invalid physical frame number %d\n", pfn);
        return;
    }

    // Check if the frame was actually allocated before freeing
    if (frame_bitMap[pfn] == 0) {
        TracePrintf(0, "free_frame: WARNING: Attempted to free an already free frame %d\n", pfn);
        return;
    }

    frame_bitMap[pfn] = 0; // Mark the frame as free
    TracePrintf(5, "free_frame: Freed frame %d\n", pfn);
}


int SetKernelBrk(void *addr) {
    TracePrintf(1, "SetKernelBrk: Called with addr=%p, current kernel_brk=%p, vm_enabled=%d\n",
                addr, kernel_brk, vm_enabled);

    // Validate the requested address: Must be within Region 0 and not conflict with stack.
    if (addr < (void*)_first_kernel_data_page || addr >= (void *)KERNEL_STACK_BASE) { // Assuming _first_kernel_data_page is the lower bound.
        TracePrintf(0, "SetKernelBrk: ERROR: Requested address %p out of bounds for kernel heap (min %p, max %p)\n",
                    addr, (void*)_first_kernel_data_page, (void *)KERNEL_STACK_BASE);
        return ERROR;
    }

    if (!vm_enabled) {
        // Before VM enabled: just track the kernel break within its valid range.
        // We only allow increasing the break, as deallocation isn't managed here.
        if (addr > kernel_brk) {
            kernel_brk = addr;
            return 0;
        } else if (addr == kernel_brk) {
            return 0; // No change
        } else {
            TracePrintf(0, "SetKernelBrk: ERROR: Cannot decrease kernel break before VM is enabled.\n");
            return ERROR; // Disallow decreasing break before VM enabled for simplicity.
        }
    } else {
        // After VM enabled: need to actually allocate/deallocate frames and update page tables.
        void *old_brk_page_aligned = UP_TO_PAGE((unsigned int)kernel_brk); // Current heap end, page-aligned
        void *new_brk_page_aligned = UP_TO_PAGE((unsigned int)addr);       // Requested heap end, page-aligned

        // If decreasing break, deallocate pages and update PTEs
        if (new_brk_page_aligned < old_brk_page_aligned) {
            TracePrintf(1, "SetKernelBrk: Decreasing kernel heap from %p to %p\n", old_brk_page_aligned, new_brk_page_aligned);
            for (void *p = new_brk_page_aligned; p < old_brk_page_aligned; p += PAGESIZE) {
                int vpn = (int)p >> PAGESHIFT; // Virtual page number

                // Get the PTE for this page
                pte_t *current_pte = region0_pt + vpn; // Assumes region0_pt is kernel-mapped pointer

                if (current_pte->valid) { // Check if the page was valid before attempting to free
                    int pfn_to_free = current_pte->pfn;
                    free_frame(pfn_to_free); // Return the physical frame
                    frame_bitMap[pfn_to_free] = 0; // Mark as free in bitmap

                    // Invalidate the PTE
                    current_pte->valid = 0;
                    current_pte->prot = 0;
                    current_pte->pfn = 0; // Clear PFN
                } else {
                    TracePrintf(0, "SetKernelBrk: WARNING: Attempted to deallocate unmapped page %p\n", p);
                }

                // Flush the TLB for this page to invalidate the old mapping
                WriteRegister(REG_TLB_FLUSH, (unsigned int)p);
            }
        }
        
        else if (new_brk_page_aligned > old_brk_page_aligned) { // If increasing break, map new frames
            TracePrintf(1, "SetKernelBrk: Increasing kernel heap from %p to %p\n", old_brk_page_aligned, new_brk_page_aligned);
            for (void *p = old_brk_page_aligned; p < new_brk_page_aligned; p += PAGESIZE) {
                int vpn = (int)p >> PAGESHIFT; // Virtual page number

                int pfn = allocate_frame(); // Get a free physical frame
                if (pfn == ERROR) {
                    TracePrintf(0, "SetKernelBrk: ERROR: Out of physical memory when expanding kernel heap at %p\n", p);
                    return ERROR;
                }
                frame_bitMap[pfn] = 1; // Mark as used in bitmap

                // Update the page table entry for this virtual page
                pte_t *current_pte = region0_pt + vpn; // Assumes region0_pt is kernel-mapped pointer
                current_pte->valid = 1;
                // Kernel heap needs read/write. If you have executable code on heap, add PROT_EXEC.
                current_pte->prot = PROT_READ | PROT_WRITE;
                current_pte->pfn = pfn;

                // Flush the TLB for this page
                WriteRegister(REG_TLB_FLUSH, (unsigned int)p);
                TracePrintf(1, "SetKernelBrk: Mapped kernel heap page %p (VPN %d) to PFN %d\n", p, vpn, pfn);
            }
        }
        kernel_brk = addr;
        TracePrintf(1, "SetKernelBrk: Updated kernel_brk to %p\n", kernel_brk);
        return 0;
    }
}


void init_page_table(int kernel_text_start, int kernel_data_start, int kernel_brk_start, unsigned int pmem_size) {
    TracePrintf(0, "Initializing page table...\n");

    // Dynamically allocate frame_bitMap based on the actual physical memory size.
    unsigned int num_physical_frames = pmem_size / PAGESIZE;
    frame_bitMap = (int *)calloc(num_physical_frames, sizeof(int)); // Use calloc to zero-initialize the bitmap
    if (frame_bitMap == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate frame_bitMap\n");
    }

    // Mark physical frame 0 as reserved (it's often used for initial kernel setup or left unused for safety).
    if (num_physical_frames > 0) { // Ensure there's at least one frame
        frame_bitMap[0] = 1;
    }

    TracePrintf(0, "Region 0 page table base physical address: %p\n", region0_pt);

    // Initialize all entries in the Region 0 page table to invalid.
    for (int i = 0; i < MAX_PT_LEN; i++) {
        region0_pt[i].valid = 0;
        region0_pt[i].prot = 0;
        region0_pt[i].pfn = 0;
    }

    // Initialize physical frames used by the kernel's initial load.
    for (int i = 0; i < kernel_text_start && i < num_physical_frames; i++) {
        frame_bitMap[i] = 1;
    }

    // Initialize mappings for kernel text, data, and heap sections in the Region 0 page table.
    for (int vpn_index = kernel_text_start; vpn_index < kernel_brk_start; vpn_index++) {
        // Safety checks to ensure we don't access out of bounds for the page table or physical memory.
        if (vpn_index >= MAX_PT_LEN) {
            TracePrintf(0, "ERROR: Attempted to map VPN %d which is beyond MAX_PT_LEN in Region 0 (kernel text/data/heap).\n", vpn_index);
            break;
        }
        if (vpn_index >= num_physical_frames) {
            TracePrintf(0, "ERROR: Attempted to identity map virtual page %d to physical frame %d which is beyond pmem_size.\n", vpn_index, vpn_index);
            break;
        }

        pte_t *entry = &region0_pt[vpn_index]; // Access the PTE directly by VPN index
        entry->valid = 1;
        if (vpn_index < kernel_data_start) {
            entry->prot = PROT_READ | PROT_EXEC; // Text section: read and execute permissions
        } else {
            entry->prot = PROT_READ | PROT_WRITE; // Data/heap sections: read and write permissions
            TracePrintf(0, "Kernel data/heap permission for page: %d is %d\n", vpn_index, entry->prot);
        }
        entry->pfn = vpn_index; // Identity mapping: virtual page number = physical frame number
        frame_bitMap[entry->pfn] = 1; // Mark the corresponding physical frame as used
    }
    TracePrintf(0, "Kernel text, data, and heap initialized with %d total entries\n", kernel_brk_start - kernel_text_start);

    // Set the kernel break (heap top) using SetKernelBrk.
    void *kernel_brk = UP_TO_PAGE(kernel_brk_start * PAGESIZE);
    if (SetKernelBrk(kernel_brk) != 0) {
        TracePrintf(0, "ERROR: Failed to set kernel break to address %p\n", kernel_brk);
    }

    // Initialize mappings for the kernel stack in the Region 0 page table.
    int base_kernelStack_vpn_index = KERNEL_STACK_BASE / PAGESIZE;
    int kernelStack_limit_vpn_index = KERNEL_STACK_LIMIT / PAGESIZE;
    for (int vpn_index = base_kernelStack_vpn_index; vpn_index < kernelStack_limit_vpn_index; vpn_index++) {
        // Safety checks
        if (vpn_index >= MAX_PT_LEN) {
            TracePrintf(0, "ERROR: Attempted to map Kernel Stack VPN %d which is beyond MAX_PT_LEN in Region 0.\n", vpn_index);
            break;
        }
        if (vpn_index >= num_physical_frames) {
            TracePrintf(0, "ERROR: Attempted to identity map Kernel Stack virtual page %d to physical frame %d which is beyond pmem_size.\n", vpn_index, vpn_index);
            break;
        }
        pte_t *entry = &region0_pt[vpn_index]; // Access the PTE directly by index
        entry->valid = 1;
        entry->prot = PROT_READ | PROT_WRITE; // Kernel stack: read and write permissions
        entry->pfn = vpn_index; // Identity mapping for kernel stack
        frame_bitMap[entry->pfn] = 1; // Mark the corresponding physical frame as used
    }
    TracePrintf(0, "Kernel stack initialized with %d total entries\n", (KERNEL_STACK_LIMIT - KERNEL_STACK_BASE) / PAGESIZE);

    // Set hardware registers for Region 0 page table.
    int numPages_inRegion0 = VMEM_0_LIMIT / PAGESIZE; // Total number of pages in Region 0
    WriteRegister(REG_PTBR0, (unsigned int)region0_pt); // Set base physical address of Region 0 page table (now a static array address)
    WriteRegister(REG_PTLR0, (unsigned int)numPages_inRegion0); // Set number of entries in Region 0 page table


    /* --------------------------------------Region 1 page table ------------------------------------- */
    // Initialize all entries in the Region 1 page table to invalid.
    for (int i = 0; i < MAX_PT_LEN; i++) {
        region1_pt[i].valid = 0;
        region1_pt[i].prot = 0;
        region1_pt[i].pfn = 0;
    }

    // Initialize the first entry for Region 1.
    int first_region1_vpn_index = 0;

    pte_t *entry_r1 = &region1_pt[first_region1_vpn_index]; // Access the first PTE
    entry_r1->valid = 1;
    entry_r1->prot = PROT_READ | PROT_WRITE | PROT_EXEC; // Assuming user code will need read, write, and execute

    // Ensure the physical frame number for Region 1's first mapped page is within bounds.
    if (numPages_inRegion0 >= num_physical_frames) {
        TracePrintf(0, "ERROR: Cannot map Region 1's first page. No physical frames available after kernel space (PFN %d >= total frames %d).\n", numPages_inRegion0, num_physical_frames);
        entry_r1->valid = 0; // Invalidate the entry if no space
    } else {
        entry_r1->pfn = numPages_inRegion0; // Map to the first available physical frame after kernel's mapped space
        frame_bitMap[entry_r1->pfn] = 1; // Mark the *physical frame* as used in the bitmap
        TracePrintf(0, "Region 1 page table initialized with 1 total entry, mapping virtual page %d to physical frame %d\n", first_region1_vpn_index, entry_r1->pfn);
    }

    // Set hardware registers for Region 1 page table.
    WriteRegister(REG_PTBR1, (unsigned int)region1_pt); // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, (unsigned int)1); // Set limit of Region 1 page table to 1 entry initially
    TracePrintf(0, "Wrote region 1 page table base register to %p\n", region1_pt);
    TracePrintf(0, "Wrote region 1 page table limit register to %u\n", (unsigned int)1);
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


void map_page(pte_t *page_table_base, int vpn, int pfn, int prot) {
    // Calculate the address of the specific Page Table Entry (PTE)
    // by adding the virtual page number (VPN) as an offset to the page table base.
    // Since page_table_base is of type pte_t*, adding 'vpn' automatically
    // accounts for sizeof(pte_t) for each step (pointer arithmetic).
    pte_t *entry = page_table_base + vpn;

    // Set the valid bit to 1, indicating this is a valid mapping.
    entry->valid = 1;

    // Set the protection bits (read, write, execute) as specified.
    entry->prot = prot;

    // Set the Physical Frame Number (PFN) this virtual page maps to.
    entry->pfn = pfn;

    // Construct the virtual address for the page to flush the TLB.
    // This is done by shifting the VPN left by PAGESHIFT to get the page's base address.
    unsigned int virtual_address_to_flush = (unsigned int)vpn << PAGESHIFT;

    // Flush the Translation Lookaside Buffer (TLB) entry for this virtual page.
    // This ensures that the MMU re-reads the updated page table entry on next access.
    WriteRegister(REG_TLB_FLUSH, virtual_address_to_flush);
}



void unmap_page(pte_t *page_table_base, int vpn) {
    // Calculate the address of the specific Page Table Entry (PTE)
    // by adding the virtual page number (VPN) as an offset to the page table base.
    pte_t *entry = page_table_base + vpn;

    // Invalidate the page table entry by setting the valid bit to 0.
    // This tells the MMU that this virtual page is no longer mapped.
    entry->valid = 0;

    // It's good practice to also clear other bits like protection and PFN
    // when unmapping, although setting valid=0 is the primary action to unmap.
    entry->prot = 0;
    entry->pfn = 0; // Or a sentinel value like 0 if frame 0 is never allocated

    // Construct the virtual address for the page to flush the TLB.
    unsigned int virtual_address_to_flush = (unsigned int)vpn << PAGESHIFT;

    // Flush the Translation Lookaside Buffer (TLB) entry for this virtual page.
    // This is crucial to ensure the MMU does not use a stale, cached mapping
    // for this page after it has been unmapped in the page table.
    WriteRegister(REG_TLB_FLUSH, virtual_address_to_flush);
}


void *setup_temp_mapping(int pfn) {
    // Define a virtual address for temporary mapping. This should be a page
    // that is guaranteed to be unused and available for temporary mappings.
    // For example, one page below the kernel stack base.
    void *temp_vaddr = (void *)(KERNEL_STACK_BASE - PAGESIZE);
    struct pte *region0_pt = get_region0_pt(); // Get pointer to Region 0 page table

    TracePrintf(5, "setup_temp_mapping: Mapping pfn %d to temporary vaddr %p.\n", pfn, temp_vaddr);

    // Check if the temporary virtual address is already mapped
    // This is a simplified check; a more robust solution would track temporary mappings
    // or use a dedicated temporary mapping page.
    if (get_physical_frame(temp_vaddr, region0_pt) != -1) {
        TracePrintf(0, "setup_temp_mapping: ERROR: Temporary mapping vaddr %p is already in use.\n", temp_vaddr);
        return NULL; // Or unmap it first if it's safe to do so
    }

    // Map the physical frame to the temporary virtual address in Region 0
    map_page(region0_pt, (int)((unsigned int)temp_vaddr >> PAGESHIFT), pfn, PROT_READ | PROT_WRITE);

    // Flush TLB for the specific temporary mapping to ensure consistency
    WriteRegister(REG_TLB_FLUSH, (unsigned int)temp_vaddr);

    return temp_vaddr;
}


void remove_temp_mapping(void *addr) {
    struct pte *region0_pt = get_region0_pt(); // Get pointer to Region 0 page table

    TracePrintf(5, "remove_temp_mapping: Unmapping temporary vaddr %p.\n", addr);

    // Unmap the virtual page
    unmap_page(region0_pt, (int)((unsigned int)addr >> PAGESHIFT));

    // Flush TLB for the specific temporary mapping
    WriteRegister(REG_TLB_FLUSH, (unsigned int)addr);
}


int get_physical_frame(void *addr, pte_t *pt) {
    unsigned int vaddr = (unsigned int)addr;
    pte_t *page_table_base = NULL;
    int vpn; // Virtual Page Number

    // Determine the correct page table base and calculate the Virtual Page Number (VPN)
    if (vaddr >= VMEM_0_BASE) {
        // Address is in Region 1 (User Space)
        if (pt != NULL) {
            // An explicit page table was provided; use it.
            // This is useful for looking up addresses in a specific process's (other than current) Region 1.
            page_table_base = pt;
        } else {
            // No explicit page table provided, so use the current process's Region 1 page table.
            if (current_process == NULL || current_process->region1_pt == NULL) {
                TracePrintf(0, "get_physical_frame: ERROR: No current process or its Region 1 page table for user address 0x%x\n", vaddr);
                return ERROR;
            }
            page_table_base = current_process->region1_pt;
        }
        // Calculate VPN for Region 1: subtract the base of Region 1 and then shift.
        vpn = (vaddr - VMEM_0_BASE) >> PAGESHIFT;
    } else {
        // Address is in Region 0 (Kernel Space)
        // Region 0 virtual addresses always map through the global kernel Region 0 page table.
        // The 'pt' parameter is disregarded for Region 0 lookups.
        page_table_base = region0_pt;
        // Calculate VPN for Region 0: simply shift the address.
        vpn = vaddr >> PAGESHIFT;
    }

    // Sanity check: Ensure a valid page table base was successfully determined.
    if (page_table_base == NULL) {
        TracePrintf(0, "get_physical_frame: ERROR: Failed to determine valid page table base for virtual address 0x%x\n", vaddr);
        return ERROR;
    }

    // Access the corresponding Page Table Entry (PTE) using pointer arithmetic.
    pte_t *entry = page_table_base + vpn;

    // Check if the page table entry is valid (i.e., the page is mapped).
    if (!entry->valid) {
        TracePrintf(4, "get_physical_frame: Virtual address 0x%x (VPN %d) is not mapped (PTE invalid).\n", vaddr, vpn);
        return ERROR; // Page is not mapped
    }

    // If the PTE is valid, return the Physical Frame Number (PFN) it contains.
    TracePrintf(4, "get_physical_frame: Virtual address 0x%x (VPN %d) maps to PFN %d.\n", vaddr, vpn, entry->pfn);
    return entry->pfn;
}

// TODO: create kernel_malloc, kernel_free, and perhaps memset
