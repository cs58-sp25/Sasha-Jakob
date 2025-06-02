/**
 * Date: 5/4/25
 * File: memory.h
 * Description: Memory header file for Yalnix OS
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <hardware.h>
#include "pcb.h"


extern int vm_enabled;  // Flag indicating if virtual memory is enabled
extern void *kernel_brk;  // Current kernel break address
extern void *user_brk;    // Current user break address
extern int *frame_bitMap;      // Bitmap to track free/used frames
extern pte_t region0_pt[]; // Page table for Region 0

#define ADDR_TO_PAGE_NUM(addr) ((unsigned int)(addr) >> PAGESHIFT)
#define PAGE_NUM_TO_ADDR(page_num) ((void *)((unsigned int)(page_num) << PAGESHIFT))

void cpyuc(UserContext *dest, UserContext *src);

/**
 * @brief Allocate a free physical frame
 *
 * Iterates through the global frame_bitMap to find an unused physical frame.
 * Marks the found frame as used and returns its physical frame number (pfn).
 *
 * @return Physical frame number on success, -1 if no free frames are available.
 */
int allocate_frame(void);


/**
 * @brief Release a physical frame back to the free pool
 *
 * Marks the specified physical frame number (pfn) as free in the global frame_bitMap.
 * Performs basic validation to ensure the pfn is within valid bounds and was previously allocated.
 *
 * @param pfn Physical frame number to free.
 */
void free_frame(int pfn);


/**
 * @brief Sets the kernel's program break (heap end) address.
 *
 * This function manages the kernel heap. Before virtual memory is enabled,
 * it simply tracks the `kernel_brk` pointer. After virtual memory is enabled,
 * it dynamically allocates/deallocates physical pages and updates the
 * Region 0 page table as needed.
 *
 * @param addr The new desired program break address.
 * @return 0 on success, ERROR on failure (e.g., out of memory, invalid address).
 */
int SetKernelBrk(void *addr);


/**
 * Initialize the Region 0 and Region 1 page tables
 * Maps kernel text, data, and stack with appropriate permissions
 *
 * @param kernel_text_start First page of kernel text
 * @param kernel_data_start First page of kernel data
 * @param kernel_brk_start First page after kernel data
 */
void init_region0_pageTable(int kernel_text_start, int kernel_data_start, int kernel_brk_start, unsigned int pmem_size);



/**
 * enable_virtual_memory - Enables the MMU
 *
 * Writes to REG_VM_ENABLE to turn on virtual memory.
 * This is a critical step as all addresses are interpreted as
 * virtual after this point.
 */
void enable_virtual_memory(void);


pte_t *InitializeKernelStack();


/**
 * @brief Maps a virtual page to a physical frame in a given page table.
 *
 * This function takes the base address of a page table, a virtual page number (VPN),
 * a physical frame number (PFN), and desired protection bits, then updates the
 * corresponding page table entry and flushes the TLB for that virtual page.
 *
 * @param page_table_base A pointer to the base of the page table (e.g., region0_pt or region1_pt).
 * @param vpn The virtual page number to map.
 * @param pfn The physical frame number to map to.
 * @param prot The protection bits for the page (e.g., PROT_READ | PROT_WRITE).
 */
void map_page(pte_t *page_table_base, int vpn, int pfn, int prot);


/**
 * @brief Unmaps a virtual page from a given page table.
 *
 * This function invalidates a page table entry for a given virtual page number (VPN)
 * in the specified page table and flushes the TLB for that virtual page.
 *
 * @param page_table_base A pointer to the base of the page table (e.g., region0_pt or region1_pt).
 * @param vpn The virtual page number to unmap.
 */
void unmap_page(pte_t *page_table_base, int vpn);

#endif /* _MEMORY_H_ */
