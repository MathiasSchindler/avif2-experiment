#include <stddef.h>

__attribute__((optnone))
void bzero(void *memory, size_t size) {
    volatile unsigned char *bytes = (volatile unsigned char *)memory;
    while (size != 0U) {
        *bytes++ = 0U;
        --size;
    }
}
