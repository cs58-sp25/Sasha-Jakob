#include "frames.h"

#include <hardware.h>
#include <yalnix.h>
#include <ykernel.h>

int *frame_bitMap;  // Bitmap to track free/used frames

static void print_integer_array(int *array, int size, const char *array_name);

int allocate_frame(void) {
    TracePrintf(1, "Enter allocate_frame()\n");

    // // Call the print function
    // int array_size = 25;
    // print_integer_array(frame_bitMap, array_size, "frame_bitMap");

    // Iterate through the frame_bitMap to find a free frame
    for (int i = 0; i < NUM_VPN; i++) {  // Assuming NUM_VPN is the total number of physical frames
        if (frame_bitMap[i] == 0) {      // If the frame is free (0 means free, 1 means used)
            frame_bitMap[i] = 1;         // Mark it as used
            TracePrintf(1, "allocate_frame: Allocated frame %d\n", i);
            return i;  // Return the physical frame number
        }
    }
    TracePrintf(0, "allocate_frame: ERROR: No free physical frames available\n");
    return ERROR;  // No free frames
}

void free_frame(int pfn) {
    // Basic validation for the physical frame number
    if (pfn < 0 || pfn >= NUM_VPN) {  // Assuming NUM_VPN is the total number of physical frames
        TracePrintf(0, "free_frame: ERROR: Invalid physical frame number %d\n", pfn);
        return;
    }

    // Check if the frame was actually allocated before freeing
    if (frame_bitMap[pfn] == 0) {
        TracePrintf(0, "free_frame: WARNING: Attempted to free an already free frame %d\n", pfn);
        return;
    }

    frame_bitMap[pfn] = 0;  // Mark the frame as free
    TracePrintf(0, "free_frame: Freed frame %d\n", pfn);
}

// Helper function for printing the frame bitmap
static void print_integer_array(int *array, int size, const char *array_name) {
    if (array_name) {
        TracePrintf(0, "Array '%s' contents (size: %d):\n", array_name, size);
    } else {
        TracePrintf(0, "Array contents (size: %d):\n", size);
    }

    for (int i = 0; i < size; i++) {
        // Print each element. You can adjust the format as needed.
        // For example, to print elements separated by commas:
        TracePrintf(0, "  [%d]: %d%s\n", i, array[i], (i == size - 1) ? "" : ",");
    }
    TracePrintf(0, "\n");  // Newline at the end for clean formatting
}
