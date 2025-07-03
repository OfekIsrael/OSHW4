#include <cstddef>
#include <unistd.h>

struct MallocMataData {
    size_t size;
    bool is_free;
    MallocMataData* next;
    MallocMataData* prev;
};

MallocMataData* head = nullptr;
void* start = sbrk(0);

void* smalloc(size_t size) {
    MallocMataData* current = head;
    void* current_ptr = start
    while(current != nullptr) {
        current_ptr += (void*) current->size;
        if(current->size >= size) break;
    }
    if(current != nullptr) {
        
    }
}

