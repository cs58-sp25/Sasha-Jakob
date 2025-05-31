/**
 * Date: 5/4/25
 * File: memory.c
 * Description: Memory management implementation for Yalnix OS
 */

#include "memory.h"
#include <hardware.h>
#include <yalnix.h>


int vm_enabled = 0;       // Initialize to 0 (VM disabled) by default
void *kernel_brk = NULL;  // Current kernel break address
void *user_brk = NULL;    // Current user break address
int *frame_bitMap;      // Bitmap to track free/used frames
pte_t region0_pt[MAX_PT_LEN]; // Page table for Region 0


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
    TracePrintf(1, "Enter free_frame() for physical page number %d\n", pfn);
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
        // Before VM enabled: just track the kernel break within its valid range
        // We only allow increasing the break, as deallocation isn't managed here
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
        void *old_brk_page_aligned = (void *)(UP_TO_PAGE(kernel_brk) * PAGESIZE); // Current heap end, page-aligned
        void *new_brk_page_aligned = (void *)(UP_TO_PAGE(addr) * PAGESIZE);       // Requested heap end, page-aligned

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


void init_region0_pageTable(int kernel_text_start, int kernel_data_start, int kernel_brk_start, unsigned int pmem_size) {
    TracePrintf(0, "Initializing page table...\n");

    // Dynamically allocate frame_bitMap based on the actual physical memory size.
    unsigned int num_physical_frames = pmem_size / PAGESIZE;
    frame_bitMap = (int *)calloc(num_physical_frames, sizeof(int)); // Use calloc to zero-initialize the bitmap
    if (frame_bitMap == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate frame_bitMap\n");
    }

    TracePrintf(0, "Region 0 page table base physical address: %p\n", region0_pt);

    // Initialize all entries in the Region 0 page table to invalid.
    for (int i = 0; i < MAX_PT_LEN; i++) {
        unmap_page(region0_pt, i);
    }
    frame_bitMap[0] = 1;

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
            map_page(region0_pt, vpn_index, vpn_index, PROT_READ | PROT_EXEC);
        } else {
            map_page(region0_pt, vpn_index, vpn_index, PROT_READ | PROT_WRITE);
            TracePrintf(0, "Kernel data/heap permission for page: %d is %d\n", vpn_index, entry->prot);
        }
    }
    TracePrintf(0, "Kernel text, data, and heap initialized with %d total entries\n", kernel_brk_start - kernel_text_start);


    // Initialize mappings for the kernel stack in the Region 0 page table.
    int base_kernelStack_vpn_index = KERNEL_STACK_BASE >> PAGESHIFT;
    int kernelStack_limit_vpn_index = KERNEL_STACK_LIMIT >> PAGESHIFT;
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
        map_page(region0_pt, vpn_index, vpn_index, PROT_READ | PROT_WRITE);
    }
    TracePrintf(0, "Kernel stack initialized with %d total entries\n", (KERNEL_STACK_LIMIT - KERNEL_STACK_BASE) / PAGESIZE);

    // Set hardware registers for Region 0 page table.
    int numPages_inRegion0 = VMEM_0_LIMIT / PAGESIZE; // Total number of pages in Region 0
    WriteRegister(REG_PTBR0, (u_long)region0_pt); // Set base physical address of Region 0 page table (now a static array address)
    WriteRegister(REG_PTLR0, (unsigned int)numPages_inRegion0); // Set number of entries in Region 0 page table
}


void enable_virtual_memory(void) {
    WriteRegister(REG_VM_ENABLE, 1); // Write 1 to REG_VM_ENABLE register to enable virtual memory
    vm_enabled = 1; // Update global flag to track that VM is now enabled
    TracePrintf(0, "Virtual memory enabled\n");
}

pte_t *InitializeKernelStack(){
  pte_t *kernel_stack = (pte_t *)malloc((KERNEL_STACK_MAXSIZE / PAGESIZE) * sizeof(pte_t));
  if (kernel_stack == NULL){
    TracePrintf(0, "Failed to allocate kernel stack\n");
    Halt();
  }

  for (int j = 0; j < (KERNEL_STACK_MAXSIZE / PAGESIZE); j++){
    int vpage = (KERNEL_STACK_BASE >> PAGESHIFT) + j;
    int pfn = allocate_frame();
    map_page(kernel_stack, vpage, pfn, PROT_READ | PROT_WRITE); // Map the kernel stack pages to themselves
  }
  return kernel_stack;
}


void map_page(pte_t *page_table_base, int vpn, int pfn, int prot) {
    pte_t *entry = page_table_base + vpn; // Calculate the address of the specific Page Table Entry (PTE)
    entry->valid = 1;  // Set the valid bit to 1 or 0 as specified
    entry->prot = prot; // Set the protection bits (read, write, execute) as specified
    entry->pfn = pfn; // Set the Physical Frame Number (PFN) this virtual page maps to
    frame_bitMap[entry->pfn] = 1; // Mark the corresponding physical frame as used
}



void unmap_page(pte_t *page_table_base, int vpn) {
    pte_t *entry = page_table_base + vpn;
    // Invalidate the page table entry by setting the valid bit to 0.
    entry->valid = 0;
    entry->prot = 0;
    entry->pfn = 0;
    frame_bitMap[entry->pfn] = 0; // Mark the corresponding physical frame as free
}
