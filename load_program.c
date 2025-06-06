#include <ctype.h>
#include <fcntl.h>
#include <hardware.h>
#include <load_info.h>
#include <unistd.h>
#include <yalnix.h>
#include <ykernel.h>
#include <ylib.h>
#include <yuser.h>

#include "memory.h"
#include "pcb.h"

/*
 * ==>> #include anything you need for your kernel here
 */

/*
 *  Load a program into an existing address space.  The program comes from
 *  the Linux file named "name", and its arguments come from the array at
 *  "args", which is in standard argv format.  The argument "proc" points
 *  to the process or PCB structure for the process into which the program
 *  is to be loaded.
 */

/*
 * ==>> Declare the argument "proc" to be a pointer to the PCB of
 * ==>> the current process.
 */
int LoadProgram(char *name, char *args[], pcb_t *proc) {
    TracePrintf(1, "Enter LoadProgram.\n");
    int fd;
    int (*entry)();
    struct load_info li;
    int i;
    char *cp;
    char **cpp;
    char *cp2;
    int argcount;
    int size;
    int text_pg1;
    int data_pg1;
    int data_npg;
    int stack_npg;
    long segment_size;
    char *argbuf;

    /*
     * Open the executable file
     */
    if ((fd = open(name, O_RDONLY)) < 0) {
        TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
        return ERROR;
    }

    if (LoadInfo(fd, &li) != LI_NO_ERROR) {
        TracePrintf(0, "LoadProgram: '%s' not in Yalnix format\n", name);
        close(fd);
        return ERROR;
    }

    if (li.entry < VMEM_1_BASE) {
        TracePrintf(0, "LoadProgram: '%s' not linked for Yalnix\n", name);
        close(fd);
        return ERROR;
    }

    /*
     * Figure out in what region 1 page the different program sections
     * start and end
     */
    text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT;
    data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT;
    data_npg = li.id_npg + li.ud_npg;
    /*
     *  Figure out how many bytes are needed to hold the arguments on
     *  the new stack that we are building.  Also count the number of
     *  arguments, to become the argc that the new "main" gets called with.
     */
    size = 0;
    for (i = 0; args[i] != NULL; i++) {
        TracePrintf(3, "counting arg %d = '%s'\n", i, args[i]);
        size += strlen(args[i]) + 1;
    }
    argcount = i;

    TracePrintf(2, "LoadProgram: argsize %d, argcount %d\n", size, argcount);

    /*
     *  The arguments will get copied starting at "cp", and the argv
     *  pointers to the arguments (and the argc value) will get built
     *  starting at "cpp".  The value for "cpp" is computed by subtracting
     *  off space for the number of arguments (plus 3, for the argc value,
     *  a NULL pointer terminating the argv pointers, and a NULL pointer
     *  terminating the envp pointers) times the size of each,
     *  and then rounding the value *down* to a double-word boundary.
     */
    cp = ((char *)VMEM_1_LIMIT) - size;

    cpp = (char **)(((int)cp - ((argcount + 3 + POST_ARGV_NULL_SPACE) * sizeof(void *))) & ~7);

    /*
     * Compute the new stack pointer, leaving INITIAL_STACK_FRAME_SIZE bytes
     * reserved above the stack pointer, before the arguments.
     */
    cp2 = (caddr_t)cpp - INITIAL_STACK_FRAME_SIZE;

    TracePrintf(1, "prog_size %d, text %d data %d bss %d pages\n", li.t_npg + data_npg, li.t_npg, li.id_npg, li.ud_npg);

    /*
     * Compute how many pages we need for the stack */
    stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;

    TracePrintf(1, "LoadProgram: heap_size %d, stack_size %d\n", li.t_npg + data_npg, stack_npg);

    /* leave at least one page between heap and stack */
    if (stack_npg + data_pg1 + data_npg >= MAX_PT_LEN) {
        close(fd);
        return ERROR;
    }

    /*
     * This completes all the checks before we proceed to actually load
     * the new program.  From this point on, we are committed to either
     * loading succesfully or killing the process.
     */

    /*
     * Set the new stack pointer value in the process's UserContext
     */

    /*
     * ==>> (rewrite the line below to match your actual data structure)
     * ==>> proc->uc.sp = cp2;
     */
    // Rewrite the line from above to do what it's supposed to
    proc->user_context.sp = cp2;

    /*
     * Now save the arguments in a separate buffer in region 0, since
     * we are about to blow away all of region 1.
     */
    cp2 = argbuf = (char *)malloc(size);

    /*
     * ==>> You should perhaps check that malloc returned valid space
     */
    // Check to see if the malloc failed
    if (cp2 == NULL) {
        TracePrintf(1, "ERROR, malloc for cp2 failed in LoadProgram.\n");
        return ERROR;
    }

    for (i = 0; args[i] != NULL; i++) {
        TracePrintf(3, "saving arg %d = '%s'\n", i, args[i]);
        strcpy(cp2, args[i]);
        cp2 += strlen(cp2) + 1;
    }

    /*
     * Set up the page tables for the process so that we can read the
     * program into memory.  Get the right number of physical pages
     * allocated, and set them all to writable.
     */

    /* ==>> Throw away the old region 1 virtual address space by
     * ==>> curent process by walking through the R1 page table and,
     * ==>> for every valid page, free the pfn and mark the page invalid.
     */
    // Loop over the region 1 page table
    free_userspace(proc);

    /*
     * ==>> Then, build up the new region1.
     * ==>> (See the LoadProgram diagram in the manual.)
     */

    /*
     * ==>> First, text. Allocate "li.t_npg" physical pages and map them starting at
     * ==>> the "text_pg1" page in region 1 address space.
     * ==>> These pages should be marked valid, with a protection of
     * ==>> (PROT_READ | PROT_WRITE).
     */

    TracePrintf(1, "Load_program: Allocating pages for physical text.\n");
    for (int i = text_pg1; i < text_pg1 + li.t_npg; i++) {
        int nf = allocate_frame();
        if (nf == ERROR) {
            TracePrintf(1, "ERROR, no new frames to allocate for LoadProgram.\n");
            return ERROR;
        }
        proc->region1_pt[i].valid = 1;
        proc->region1_pt[i].prot = PROT_READ | PROT_WRITE;
        proc->region1_pt[i].pfn = nf;
    }
    /*
     * ==>> Then, data. Allocate "data_npg" physical pages and map them starting at
     * ==>> the  "data_pg1" in region 1 address space.
     * ==>> These pages should be marked valid, with a protection of
     * ==>> (PROT_READ | PROT_WRITE)
     */

    TracePrintf(1, "Load_program: Allocating pages for data.\n");
    for (int i = data_pg1; i < data_pg1 + data_npg; i++) {
        int nf = allocate_frame();
        if (nf == ERROR) {
            TracePrintf(1, "ERROR, no new frames to allocate for LoadProgram.\n");
            return ERROR;
        }
        proc->region1_pt[i].valid = 1;                      // CORRECT: Modifies the actual page table entry
        proc->region1_pt[i].prot = PROT_READ | PROT_WRITE;  // CORRECT: Modifies the actual page table entry
        proc->region1_pt[i].pfn = nf;                       // CORRECT: Modifies the actual page table entry
    }
    
    proc->brk = (void *)((data_pg1 + data_npg) << PAGESHIFT);

    /*
     * ==>> Then, stack. Allocate "stack_npg" physical pages and map them to the top
     * ==>> of the region 1 virtual address space.
     * ==>> These pages should be marked valid, with a
     * ==>> protection of (PROT_READ | PROT_WRITE).
     */

    TracePrintf(1, "Load_program: Allocating pages for stack.\n");
    for (int i = MAX_PT_LEN - stack_npg; i < MAX_PT_LEN; i++) {
        int nf = allocate_frame();
        if (nf == ERROR) {
            TracePrintf(1, "ERROR, no new frames to allocate for LoadProgram.\n");
            return ERROR;
        }
        proc->region1_pt[i].valid = 1;                      // CORRECT: Modifies the actual page table entry
        proc->region1_pt[i].prot = PROT_READ | PROT_WRITE;  // CORRECT: Modifies the actual page table entry
        proc->region1_pt[i].pfn = nf;                       // CORRECT: Modifies the actual page table entry
    }

    /*
     * ==>> (Finally, make sure that there are no stale region1 mappings left in the TLB!)
     */
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    /*
     * All pages for the new address space are now in the page table.
     */

    /*
     * Read the text from the file into memory.
     */
    TracePrintf(1, "Load_program: Reading the text into memory.\n");
    lseek(fd, li.t_faddr, SEEK_SET);
    segment_size = li.t_npg << PAGESHIFT;

    if (read(fd, (void *)li.t_vaddr, segment_size) != segment_size) {
        TracePrintf(0, "Load_program. ERROR failed to do whatever read is supposed to do");
        close(fd);
        return KILL;  // see ykernel.h
    }

    /*
     * Read the data from the file into memory.
     */
    TracePrintf(1, "Load_program: Reading the data into memory.\n");
    lseek(fd, li.id_faddr, 0);
    segment_size = li.id_npg << PAGESHIFT;

    if (read(fd, (void *)li.id_vaddr, segment_size) != segment_size) {
        close(fd);
        return KILL;
    }

    close(fd); /* we've read it all now */
    /*
     * ==>> Above, you mapped the text pages as writable, so this code could write
     * ==>> the new text there.
     *
     * ==>> But now, you need to change the protections so that the machine can execute
     * ==>> the text.
     *
     * ==>> For each text page in region1, change the protection to (PROT_READ | PROT_EXEC).
     * ==>> If any of these page table entries is also in the TLB,
     * ==>> you will need to flush the old mapping.
     */

    TracePrintf(1, "Load_program: Making text read and execute only.\n");
    for (int i = text_pg1; i < text_pg1 + li.t_npg; i++) {
        proc->region1_pt[i].prot = PROT_READ | PROT_EXEC; // CORRECT: Modifies the actual page table entry
        // The TLB flush is now for an actually updated entry
        WriteRegister(REG_TLB_FLUSH, (unsigned int)VMEM_1_BASE + (i << PAGESHIFT));
    }

    /*
     * Zero out the uninitialized data area
     */
    bzero((void *)li.id_end, li.ud_end - li.id_end);
    bzero((void *)li.id_end, li.ud_end - li.id_end);

    /*
     * Set the entry point in the process's UserContext
     */

    /*
     * ==>> (rewrite the line below to match your actual data structure)
     * ==>> proc->uc.pc = (caddr_t) li.entry;
     */
    proc->user_context.pc = (caddr_t)li.entry;

    /*
     * Now, finally, build the argument list on the new stack.
     */

    memset(cpp, 0x00, VMEM_1_LIMIT - ((int)cpp));

    *cpp++ = (char *)argcount; /* the first value at cpp is argc */
    cp2 = argbuf;
    for (i = 0; i < argcount; i++) { /* copy each argument and set argv */
        *cpp++ = cp;
        strcpy(cp, cp2);
        cp += strlen(cp) + 1;
        cp2 += strlen(cp2) + 1;
    }
    free(argbuf);
    *cpp++ = NULL; /* the last argv is a NULL pointer */
    *cpp++ = NULL; /* a NULL pointer for an empty envp */

    TracePrintf(0, "Load_program: returned from load program with success\n");
    return SUCCESS;
}
