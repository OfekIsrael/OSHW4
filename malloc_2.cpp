#include <cstddef>
#include <unistd.h>
#include <limits>
#include <cstring>
#include <iostream>

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

    if (num > __SIZE_MAX__ / size) return nullptr;

    if (num * size > 100000000) return nullptr;

    void* data = smalloc(size * num);
    if(!data) return nullptr;
    std::memset(data, 0, size * num);
    return data;
}


void sfree(void* p) {
    if (p == nullptr) return;
    MallocMataData* m = (MallocMataData*) ((char*)p - sizeof(MallocMataData));
    m->is_free = true;
}


void* srealloc(void* oldp, size_t size) {

    if(size == 0 || size > 100000000) return nullptr;

    if(!oldp) {
        char* new_data = (char*)smalloc(size);
        if(!new_data) return nullptr;
        return new_data;
    }

    MallocMataData* old_m = (MallocMataData*) ((char*)oldp - sizeof(MallocMataData));
    if(size <= old_m->size) return oldp;

    MallocMataData* current = head;
    auto* old_data = (char*)oldp;

    while(current != nullptr) {

        if(current->is_free && current->size >= size) {

            current->is_free = false;

            auto* data = (char*)current + sizeof(MallocMataData);
            size_t copy_size = (old_m->size < size) ? old_m->size : size;
            std::memmove(data, old_data, copy_size);

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
    std::memmove(data, old_data, copy_size);
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
