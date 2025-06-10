
#include <hardware.h>

extern int *frame_bitMap;      // Bitmap to track free/used frames

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
