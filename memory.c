/**
 * Date: 5/4/25
 * File: memory.c
 * Description: Memory management implementation for Yalnix OS
 */

#include "memory.h"
#include <hardware.h>

int vm_enabled = 0;       // Initialize to 0 (VM disabled) by default
void *kernel_brk = NULL;  // Current kernel break address
void *user_brk = NULL;    // Current user break address
int frame_bitMap[NUM_VPN] = {0};
pte_t *region0_pt = NULL; // Pointer to the base of page table for Region 0


int allocate_frame(void) {
    // Iterate through the frame_bitMap to find a free frame
    for (int i = 0; i < NUM_VPN; i++) { // Assuming NUM_VPN is the total number of physical frames
        if (frame_bitMap[i] == 0) { // If the frame is free (0 means free, 1 means used)
            frame_bitMap[i] = 1;    // Mark it as used
            TracePrintf(5, "allocate_frame: Allocated frame %d\n", i);
            return i;               // Return the physical frame number
        }
    }
    TracePrintf(0, "allocate_frame: ERROR: No free physical frames available\n");
    return -1; // No free frames
}


void free_frame(int pfn) {
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
    if (!vm_enabled) {
        // Before VM enabled: just track the kernel break
        if (addr > kernel_brk && addr < (void *)KERNEL_STACK_BASE) {
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
            int pfn = allocate_frame(); // Get a free physical frame

            if (pfn == ERROR) {
                return ERROR; // Out of physical memory
            }

            // Update the page table entry using explicit pointer arithmetic
            pte_t *current_pte = region0_pt + vpn; // Pointer to the specific PTE
            current_pte->valid = 1;
            current_pte->prot = PROT_READ | PROT_WRITE;
            current_pte->pfn = pfn;

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
    region0_pt = (pte_t *)p_address_region0_base; // Cast to pte_t*

    // Initialize the kernel text, data, and heap sections in the page table
    for (int vpn = kernel_text_start; vpn < kernel_brk_start; vpn++) {  //
        pte_t *entry = (pte_t *)(p_address_region0_base + vpn * sizeof(pte_t));
        entry->valid = 1;
        if (vpn < kernel_data_start) { // Set protection bits appropriately
            entry->prot = PROT_READ | PROT_EXEC;  // Text section: read+execute
        } else {
            entry->prot = PROT_READ | PROT_WRITE;  // Data/heap: read+write
        }
        entry->pfn = vpn;
        frame_bitMap[entry->pfn] = 1;  // Mark the *physical frame* as used in the bitmap
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
        pte_t *entry = (pte_t *)(p_address_region0_base + vpn * sizeof(pte_t));
        entry->valid = 1;
        entry->prot = PROT_READ | PROT_WRITE;
        entry->pfn = vpn;       // Set frame numbers to the ones proceeding kernel text, data, and heap frames
        frame_bitMap[vpn] = 1;  // Mark the frame as used in the bitmap
    }
    TracePrintf(0, "Kernel stack initialized with %d total entries\n", (KERNEL_STACK_LIMIT - KERNEL_STACK_BASE) / sizeof(pte_t));

    // Set page table registers to tell hardware where to find the page table
    int numPages_inRegion0 = VMEM_0_LIMIT / PAGESIZE;  // Number of pages in Region 0
    WriteRegister(REG_PTBR0, p_address_region0_base); // Set base address of region0 page table in hardware
    WriteRegister(REG_PTLR0, numPages_inRegion0); // Set limit of region0 page table in hardware
    TracePrintf(0, "Wrote region 0 page table base register to %d\n", p_address_region0_base);
    TracePrintf(0, "Wrote region 0 page table limit register to %d\n", numPages_inRegion0);

    // Set up the base physicall adress of region 1
    unsigned int p_address_region1_base = p_address_region0_base + numPages_inRegion0 * sizeof(pte_t); // Place region1 PT at the end of region0 PT
    int vpn = VMEM_1_BASE / PAGESIZE; // Calculate the virtual page number for the first page of Region 1
    pte_t *entry = (pte_t *)(p_address_region1_base);
    entry->valid = 1;
    entry->prot = PROT_READ | PROT_WRITE;
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
