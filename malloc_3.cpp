#include <cstddef>
#include <unistd.h>
#include <limits>
#include <cstring>
#include <iostream>

#define MAX_DEG 10
#define MIN_BLOCK_SIZE 128
#define MIN_BLOCK_NUM 32


struct MallocMetaData {
    size_t size;
    unsigned int degree;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
};

static MallocMetaData* data_arr[MAX_DEG + 1];


size_t _get_block_size(unsigned int degree) {
    return MIN_BLOCK_SIZE << degree;  // 128 * 2^degree
}


void removeBlockFromArray(MallocMetaData* block) {
    if (block->next) {
        if (!block->prev) {
            data_arr[block->degree] = block->next;
        }
        block->next->prev = block->prev;
    }
    if (block->prev) {
        block->prev->next = block->next;
    }
    block->next = nullptr;
    block->prev = nullptr;
}


void addBlockToArray(MallocMetaData* block) {
    MallocMetaData* temp = data_arr[block->degree];
    if (!temp) data_arr[block->degree] = block;
    while (temp) {
        if (temp > block) {
            block->next = temp;
            block->prev = temp->prev;
            if (temp->prev) temp->prev->next = block;
            temp->prev = block;
            break;
        }
        if (!temp->next){
            block->next = nullptr;
            block->prev = temp;
            temp->next = block;
        }
        temp = temp->next;
    }
}


void* _get_buddy(MallocMetaData* block) {
    char* blockAddr = (char*)block;
    size_t blockSize = _get_block_size(block->degree);
    size_t addrVal = (size_t)blockAddr;
    size_t buddyAddr = addrVal ^ blockSize;
    return (void*)buddyAddr;
}


bool isBothFree(MallocMetaData* block) {
    if (!block->is_free) return false;
    MallocMetaData* buddy = (MallocMetaData*)_get_buddy(block);
    return buddy->degree == block->degree && buddy->is_free;
}


void uniteFreeBuddies(MallocMetaData* block) {
    if (block->degree == MAX_DEG) return;
    while (block->degree <= MAX_DEG && isBothFree(block)) {
        MallocMetaData* buddy = (MallocMetaData*)_get_buddy(block);
        removeBlockFromArray(buddy);
        block->degree++;
    }
    addBlockToArray(block);
}


void splitBuddies(void* block) {
/*
 * receives a pointer to a block and does the following:
 * 1) remove the block from the list (i).
 * 2) put split it into two blocks.
 * 3) put the buddy at list (i-1) in the appropriate place.
 */
    MallocMetaData* current = (MallocMetaData*)block;
    removeBlockFromArray(current);
    current->degree--;
    size_t buddyBlockSize = _get_block_size(current->degree);
    MallocMetaData* buddy = (MallocMetaData*) ((char*)block + buddyBlockSize);
    buddy->is_free = true;
    buddy->degree = current->degree;
    addBlockToArray(buddy);
}


void* allocateFirstTime() {
    char* ptr = (char*)sbrk(0);
    size_t max_block_size = _get_block_size(MAX_DEG);
    size_t remainder = (size_t)ptr % (MIN_BLOCK_NUM * max_block_size);
    size_t offset = 0;
    if (remainder) {
        offset = MIN_BLOCK_NUM * max_block_size - remainder;
    }
    if (sbrk(MIN_BLOCK_NUM * max_block_size + offset) == (void*)-1) {
        return nullptr;
    }
    ptr += offset;
    auto* current = (MallocMetaData*)ptr;
    data_arr[MAX_DEG] = current;
    
    for (int i = 0; i < MIN_BLOCK_NUM; i++) {
        current->degree = MAX_DEG;
        current->is_free = true;
        current->prev = (i == 0) ? nullptr : (MallocMetaData*)((char*)current - max_block_size);
        current->next = (i == MIN_BLOCK_NUM-1) ? nullptr : (MallocMetaData*)((char*)current + max_block_size);
        current = current->next;
    }
    return ptr;
}


void* smalloc(size_t size) {
    if (size == 0 || size > 100000000) return nullptr;

    MallocMetaData* current = head;
    while(current != nullptr) {
        if(current->is_free && current->size >= size) {
            current->is_free = false;
            return (char*)current + sizeof(MallocMetaData);
        }
        current = current->next;
    }
    void* value = sbrk(size + sizeof(MallocMetaData));
    if(value == (void *) -1) return nullptr;

    MallocMetaData* m = (MallocMetaData*) value;
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

    return (char*)value + sizeof(MallocMetaData);
}


void* scalloc(size_t num, size_t size) {

    if (num == 0 || size == 0) return nullptr;

    if (num > __SIZE_MAX__ / size) return nullptr;

    if (num * size > 100000000) return nullptr;

    MallocMetaData* current = head;
    bool is_zero;

    while(current != nullptr) {

        if(current->is_free && current->size >= num * size) {
            is_zero = true;
            auto* data = (char*)current + sizeof(MallocMetaData);

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

    void* value = sbrk(num * size + sizeof(MallocMetaData));
    if(value == (void *) -1) return nullptr;

    MallocMetaData* m = (MallocMetaData*) value;
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

    auto* data = (char*)m + sizeof(MallocMetaData);
    std::memset(data, 0, num * size);
    return data;
}


void sfree(void* p) {
    if (p == nullptr) return;
    MallocMetaData* m = (MallocMetaData*) ((char*)p - sizeof(MallocMetaData));
    m->is_free = true;
}


void* srealloc(void* oldp, size_t size) {

    if(size == 0 || size > 100000000) return nullptr;

    if(!oldp) return nullptr;

    MallocMetaData* old_m = (MallocMetaData*) ((char*)oldp - sizeof(MallocMetaData));
    if(size <= old_m->size) return oldp;

    MallocMetaData* current = head;
    auto* old_data = (char*)oldp;

    while(current != nullptr) {

        if(current->is_free && current->size >= size) {

            current->is_free = false;

            auto* data = (char*)current + sizeof(MallocMetaData);
            size_t copy_size = (old_m->size < size) ? old_m->size : size;
            std::memmove(data, old_data, copy_size);

            old_m->is_free = true;

            return data;
        }

        current = current->next;

    }

    void* value = sbrk(size + sizeof(MallocMetaData));
    if(value == (void *) -1) return nullptr;

    MallocMetaData* m = (MallocMetaData*) value;
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

    auto* data = (char*)m + sizeof(MallocMetaData);
    size_t copy_size = (old_m->size < size) ? old_m->size : size;
    std::memmove(data, old_data, copy_size);
    old_m->is_free = true;

    return data;

}


size_t _num_free_blocks() {

    size_t count = 0;
    MallocMetaData* current = head;

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
    MallocMetaData* current = head;

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
    MallocMetaData* current = head;

    while(current != nullptr) {
        count++;
        current = current->next;
    }

    return count;
}


size_t _num_allocated_bytes() {
    size_t count = 0;
    MallocMetaData* current = head;

    while(current != nullptr) {
        count += current->size;
        current = current->next;
    }

    return count;
}


size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetaData);
}


size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}
