#include <hardware.h>
#include <yalnix.h>
#include <ykernel.h>
#include "frames.h"

int *frame_bitMap;      // Bitmap to track free/used frames

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


