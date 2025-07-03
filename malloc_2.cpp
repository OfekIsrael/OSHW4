#include <cstddef>
#include <unistd.h>
#include <limits>

struct MallocMataData {
    size_t size;
    bool is_free;
    MallocMataData* next;
    MallocMataData* prev;
};

static MallocMataData* head = nullptr;
static MallocMataData* last = nullptr;

void* smalloc(size_t size) {
    MallocMataData* current = head;
    while(current != nullptr) {
        current_ptr += (void*) current->size;
        if(current->size >= size) break;
    }
    if(current != nullptr) {
        
    }
}

void* scalloc(size_t num, size_t size) {

    if (num == 0 || size == 0) return nullptr;

    if (num > SIZE_MAX / size) return nullptr;

    if (num * size > 100000000) return nullptr;

    MallocMataData* current = head;
    bool is_zero;

    while(current != nullptr) {

        if(current->is_free && current->size >= num * size) {
            is_zero = true;
            auto* data = (char*)current + sizeof(MallocMataData);

            for(size_t i = 0; i < num * size; i++) {
                if(data[i] != 0) {
                    is_zero = false;
                    break;
                }
            }

            if(is_zero) {
                current->is_free = false;
                return data;
            }
        }

        current = current->next;

    }

    void* value = sbrk(num * size + sizeof(MallocMataData));
    if(value == (void *) -1) return nullptr;

    MallocMataData* m = (MallocMataData*) value;
    m->size = num * size;
    m->is_free = false;
    m->next = nullptr;
    m->prev = last;

    if (!head) {
        head = m;
    } else {
        last->next = m;
    }
    last = m;

    auto* data = (char*)m + sizeof(MallocMataData);
    for(size_t i = 0; i < num * size; i++) {
        data[i] = 0;
    }
    return data;
}

void* srealloc(void* oldp, size_t size) {
    MallocMataData* m = (MallocMataData*) (oldp - sizeof(MallocMataData));
    if(size <= m->size) return oldp;

    MallocMataData* current = head;
    auto* old_data = (char*)oldp;

    while(current != nullptr) {

        if(current->is_free && current->size >= size) {

            auto* data = (char*)current + sizeof(MallocMataData);

            for(size_t i = 0; i < m->size; i++) {
                data[i] = old_data[i];
            }

            return data;
        }

        current = current->next;

    }

    void* value = sbrk(num * size + sizeof(MallocMataData));
    if(value == (void *) -1) return nullptr;

    MallocMataData* m = (MallocMataData*) value;
    m->size = num * size;
    m->is_free = false;
    m->next = nullptr;
    m->prev = last;

    if (!head) {
        head = m;
    } else {
        last->next = m;
    }
    last = m;

    auto* data = (char*)m + sizeof(MallocMataData);
    for(size_t i = 0; i < m->size; i++) {
        data[i] = old_data[i];
    }
    return data;

}
