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
extern int frame_bitMap[NUM_VPN];  // Bitmap to track free/used frames



/**
 * Allocate a free physical frame
 *
 * @return Physical frame number on success, -1 if no free frames
 */
int allocate_frame(void);


/**
 * Release a physical frame back to the free pool
 *
 * @param pfn Physical frame number to free
 */
void free_frame(int pfn);


/**
 * Sets the kernel break address
 *
 * @param addr address to set the kernel break to
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
void init_page_table(int kernel_text_start, int kernel_data_start, int kernel_brk_start);



/**
 * enable_virtual_memory - Enables the MMU
 *
 * Writes to REG_VM_ENABLE to turn on virtual memory.
 * This is a critical step as all addresses are interpreted as
 * virtual after this point.
 */
void enable_virtual_memory(void);


/**
 * Map a virtual page to a physical frame
 * Creates the mapping in the specified page table
 *
 * @param pt Pointer to page table
 * @param vpn Virtual page number
 * @param pfn Physical frame number
 * @param prot Protection bits (PROT_READ, PROT_WRITE, PROT_EXEC)
 */
void map_page(struct pte *pt, int vpn, int pfn, int prot);


/**
 * Unmap a virtual page
 * Clears the mapping in the specified page table
 *
 * @param pt Pointer to page table
 * @param vpn Virtual page number
 */
void unmap_page(struct pte *pt, int vpn);


/**
 * Copy a page table
 * Makes a complete copy of a page table, used for Fork
 *
 * @param src Source page table to copy
 * @param dest Destination to copy to
 * @param copy_frames If true, also copy the contents of mapped frames
 * @return 0 on success, ERROR on failure
 */
int copy_page_table(struct pte *src, struct pte *dest, int copy_frames);


/**
 * Handle memory fault trap
 * Checks if the fault is for stack growth, and handles accordingly
 *
 * @param uctxt User context containing fault information
 * @return 0 if handled successfully, ERROR if cannot be handled
 */
int handle_memory_trap(UserContext *uctxt);


/**
 * Allocate and map region 1 stack
 * Sets up initial user stack with desired number of pages
 *
 * @param region1_pt Pointer to Region 1 page table
 * @param num_pages Number of pages to allocate for stack
 * @return 0 on success, ERROR on failure
 */
int allocate_user_stack(struct pte *region1_pt, int num_pages);


/**
 * Allocate and map kernel stack
 * Sets up a kernel stack for a process
 *
 * @param kernel_stack_pages Array to store physical frame numbers
 * @return 0 on success, ERROR on failure
 */
int allocate_kernel_stack(pcb_t *pcb, int map_now);


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
 * Enable virtual memory
 * Writes to VM_ENABLE register to activate the MMU
 */
void enable_virtual_memory(void);


/**
 * Check if a user pointer is valid
 * Verifies that a user-provided pointer is within bounds and mapped
 *
 * @param addr Address to check
 * @param len Length of memory region to check
 * @param prot Required protection bits
 * @return 1 if valid, 0 if invalid
 */
int is_valid_user_pointer(void *addr, int len, int prot);


/**
 * Check if a user string is valid
 * Verifies that a user-provided string pointer is valid
 *
 * @param str String to check
 * @param max_len Maximum length to check
 * @return Length of string (including null) if valid, -1 if invalid
 */
int is_valid_user_string(char *str, int max_len);


/**
 * Copy data from user space to kernel space
 * Safely copies memory from user address space to kernel
 *
 * @param dst Destination address in kernel
 * @param src Source address in user space
 * @param len Number of bytes to copy
 * @return 0 on success, ERROR on failure
 */
int copy_from_user(void *dst, void *src, int len);


/**
 * Copy data from kernel space to user space
 * Safely copies memory from kernel to user address space
 *
 * @param dst Destination address in user space
 * @param src Source address in kernel
 * @param len Number of bytes to copy
 * @return 0 on success, ERROR on failure
 */
int copy_to_user(void *dst, void *src, int len);


/**
 * Get physical frame for a virtual address
 * Looks up the physical frame mapped to a virtual address
 *
 * @param addr Virtual address to look up
 * @param pt Page table to use (NULL for current)
 * @return Physical frame number or -1 if not mapped
 */
int get_physical_frame(void *addr, struct pte *pt);

#endif /* _MEMORY_H_ */