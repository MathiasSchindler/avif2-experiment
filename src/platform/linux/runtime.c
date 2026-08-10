#include <stddef.h>

void *memset(void *destination, int value, size_t size) {
    volatile unsigned char *cursor = (volatile unsigned char *)destination;
    while (size != 0U) {
        *cursor++ = (unsigned char)value;
        --size;
    }
    return destination;
}