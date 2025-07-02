#include <cstddef>
#include <unistd.h>

void* smalloc(size_t size) {
    if(size == 0 || size > 100000000) return nullptr;
    void* value = sbrk(size);
    if(value == (void *) -1) return nullptr;
    return value;
}