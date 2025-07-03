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
    if (size == 0 || size > 100000000) return nullptr;

    MallocMataData* current = head;
    while(current != nullptr) {
        if(current->is_free && current->size >= size) {
            current->is_free = false;
            current->size = size;
            return (char*)current + sizeof(MallocMataData);
        }
        current = current->next;
    }
    void* value = sbrk(size + sizeof(MallocMataData));
    if(value == (void *) -1) return nullptr;

    MallocMataData* m = (MallocMataData*) value;
    m->size = size;
    m->is_free = false;
    m->next = nullptr;
    m->prev = last;

    if (!head) {
        head = m;
    } else {
        last->next = m;
    }
    last = m;

    return (char*)value + sizeof(MallocMataData);
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


void sfree(void* p) {
    if (p == nullptr) return;
    MallocMataData* m = (MallocMataData*) ((char*)p - sizeof(MallocMataData));
    m->is_free = true;
}


void* srealloc(void* oldp, size_t size) {

    if(size == 0 || size > 100000000) return nullptr;

    if(!oldp) return nullptr;

    MallocMataData* old_m = (MallocMataData*) ((char*)oldp - sizeof(MallocMataData));
    if(size <= old_m->size) return oldp;

    MallocMataData* current = head;
    auto* old_data = (char*)oldp;

    while(current != nullptr) {

        if(current->is_free && current->size >= size) {

            current->is_free = false;

            auto* data = (char*)current + sizeof(MallocMataData);

            size_t copy_size = (old_m->size < size) ? old_m->size : size;
            for(size_t i = 0; i < copy_size; i++) {
                data[i] = old_data[i];
            }

            old_m->is_free = true;

            return data;
        }

        current = current->next;

    }

    void* value = sbrk(size + sizeof(MallocMataData));
    if(value == (void *) -1) return nullptr;

    MallocMataData* m = (MallocMataData*) value;
    m->size = size;
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
    size_t copy_size = (old_m->size < size) ? old_m->size : size;
    for(size_t i = 0; i < copy_size; i++) {
        data[i] = old_data[i];
    }
    old_m->is_free = true;

    return data;

}

size_t _num_free_blocks() {

    size_t count = 0;
    MallocMataData* current = head;

    while(current != nullptr) {
        if(current->is_free) {
            count++;
        }
        current = current->next;
    }

    return count;
}

size_t _num_free_bytes() {

    size_t count = 0;
    MallocMataData* current = head;

    while(current != nullptr) {
        if(current->is_free) {
            count += current->size;
        }
        current = current->next;
    }

    return count;
}

size_t _num_allocated_blocks() {

    size_t count = 0;
    MallocMataData* current = head;

    while(current != nullptr) {
        count++;
        current = current->next;
    }

    return count;
}

size_t _num_allocated_bytes() {
    size_t count = 0;
    MallocMataData* current = head;

    while(current != nullptr) {
        count += current->size;
        current = current->next;
    }

    return count;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMataData);
}

size_t _size_meta_data() {
    return sizeof(MallocMataData);
}
