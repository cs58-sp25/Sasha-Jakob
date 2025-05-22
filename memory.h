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
extern pte_t region1_pt[]; // Page table for Region 1


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
void init_page_table(int kernel_text_start, int kernel_data_start, int kernel_brk_start, unsigned int pmem_size);



/**
 * enable_virtual_memory - Enables the MMU
 *
 * Writes to REG_VM_ENABLE to turn on virtual memory.
 * This is a critical step as all addresses are interpreted as
 * virtual after this point.
 */
void enable_virtual_memory(void);



/**
 * @brief Setup temporary mapping for kernel stack
 *
 * Creates a temporary mapping in Region 0 to access a physical frame.
 * This is used when the kernel needs to directly access the contents of a
 * physical frame that is not normally mapped into its address space (e.g.,
 * when copying kernel stacks during Fork). The mapping is placed at a
 * predefined temporary virtual address (e.g., just below the kernel stack).
 *
 * @param pfn Physical frame number to map.
 * @return Virtual address of the temporary mapping on success, NULL on failure.
 */
void *setup_temp_mapping(int pfn);


/**
 * @brief Remove temporary mapping
 *
 * Removes a temporary mapping created by `setup_temp_mapping`.
 * This function unmaps the specified virtual address from Region 0 and
 * flushes the corresponding TLB entry.
 *
 * @param addr Virtual address of the temporary mapping to unmap.
 */
void remove_temp_mapping(void *addr);


/**
 * Map kernel stack for a process
 * Updates Region 0 page table to map the specified kernel stack frames
 *
 * @param kernel_stack_pages Array containing physical frame numbers
 * @return 0 on success, ERROR on failure
 */
int map_kernel_stack(int *kernel_stack_pages);


/**
 * Free process memory
 * Releases all memory used by a process (Region 1 + kernel stack)
 *
 * @param proc Pointer to process PCB
 */
void free_process_memory(pcb_t *proc);


/**
 * Get a pointer to the current Region 0 page table
 *
 * @return Pointer to Region 0 page table
 */
struct pte *get_region0_pt(void);

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


/**
 * Enable virtual memory
 * Writes to VM_ENABLE register to activate the MMU
 */
void enable_virtual_memory(void);


/**
 * @brief Get physical frame for a virtual address
 *
 * Looks up the physical frame mapped to a virtual address.
 *
 * @param addr Virtual address to look up.
 * @param pt Page table to use. If NULL, the function dynamically selects the
 * appropriate page table:
 * - For kernel addresses (below VMEM_0_BASE), it uses the global `region0_pt`.
 * - For user addresses (VMEM_0_BASE and above), it uses `current_process->region1_pt`.
 * If `pt` is not NULL, it is assumed to be a specific Region 1 page table
 * to use for the lookup (e.g., when inspecting another process's memory).
 * @return Physical frame number (PFN) on success, or ERROR (-1) if the page is
 * not mapped, or if an error occurs (e.g., no current process for user addresses).
 */
int get_physical_frame(void *addr, struct pte *pt);

#endif /* _MEMORY_H_ */