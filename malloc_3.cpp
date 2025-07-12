#include <cstddef>
#include <unistd.h>
#include <limits>
#include <cstring>
#include <iostream>
#include <sys/mman.h>

#define MAX_DEG 10
#define MIN_BLOCK_SIZE 128
#define MIN_BLOCK_NUM 32

struct MallocMetaData {
    unsigned int degree;
    int size;
    MallocMetaData* next;
    MallocMetaData* prev;
    bool is_mmap;
    bool is_free;
};

static MallocMetaData* data_arr[MAX_DEG + 1];
static bool is_init = false;
static int active_blocks_num;
static int bytes_allocated;


size_t _get_block_size(unsigned int degree) {
    return MIN_BLOCK_SIZE << degree;  // 128 * 2^degree
}

void _remove_block_from_arr(MallocMetaData* block) {
    if(!block) return;
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        data_arr[block->degree] = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }
    block->next = nullptr;
    block->prev = nullptr;
}

void _add_block_to_arr(MallocMetaData* block) {
    // Initialize block pointers first
    block->next = nullptr;
    block->prev = nullptr;
    block->is_free = true;

    // If list is empty, make this the head
    if (!data_arr[block->degree]) {
        data_arr[block->degree] = block;
        return;  // ← THIS RETURN WAS MISSING!
    }

    MallocMetaData* temp = data_arr[block->degree];

    // Insert at beginning if needed
    if (temp > block) {
        block->next = temp;
        temp->prev = block;
        data_arr[block->degree] = block;
        return;
    }

    // Find correct position
    while (temp->next && temp->next < block) {
        temp = temp->next;
    }

    // Insert after temp
    block->next = temp->next;
    if (temp->next) {
        temp->next->prev = block;
    }
    block->prev = temp;
    temp->next = block;
}

void* _get_buddy(MallocMetaData* block) {
    char* blockAddr = (char*)block;
    size_t blockSize = _get_block_size(block->degree);
    size_t addrVal = (size_t)blockAddr;
    size_t buddyAddr = addrVal ^ blockSize;
    return (void*)buddyAddr;
}

bool _is_both_free(MallocMetaData* block) {
    if (!block->is_free) return false;
    MallocMetaData* buddy = (MallocMetaData*)_get_buddy(block);
    return buddy->degree == block->degree && buddy->is_free;
}

void uniteFreeBuddies(MallocMetaData* block) {

    if (block->degree == MAX_DEG) return;
    while (block->degree < MAX_DEG && _is_both_free(block)) {
        MallocMetaData* buddy = (MallocMetaData*)_get_buddy(block);
        buddy->is_free = false;
        _remove_block_from_arr(buddy);
        block->degree++;
    }
    _add_block_to_arr(block);
}

void splitBuddies(void* block) {
/*
 * receives a pointer to a block and does the following:
 * 1) remove the block from the list (i).
 * 2) put split it into two blocks.
 * 3) put the buddy at list (i-1) in the appropriate place.
 */
    MallocMetaData* current = (MallocMetaData*)block;
    _remove_block_from_arr(current);
    current->degree--;
    size_t buddyBlockSize = _get_block_size(current->degree);
    MallocMetaData* buddy = (MallocMetaData*) ((char*)block + buddyBlockSize);
    buddy->is_free = true;
    buddy->degree = current->degree;
    buddy->next = nullptr;
    buddy->prev = nullptr;
    buddy->is_mmap = current->is_mmap;
    _add_block_to_arr(buddy);
}


void* allocateFirstTime() {
    active_blocks_num = 0;
    bytes_allocated = 0;
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
    MallocMetaData* current = (MallocMetaData*)ptr;
    data_arr[MAX_DEG] = current;
    
    for (int i = 0; i < MIN_BLOCK_NUM; i++) {
        current->degree = MAX_DEG;
        current->is_free = true;
        current->prev = (i == 0) ? nullptr : (MallocMetaData*)((char*)current - max_block_size);
        current->next = (i == MIN_BLOCK_NUM-1) ? nullptr : (MallocMetaData*)((char*)current + max_block_size);
        current->is_mmap = false;
        current = current->next;
    }
    return ptr;
}

void* smalloc(size_t size) {
    if (size == 0 || size > 100000000) return nullptr;

    if(!is_init) {
        allocateFirstTime();
        is_init = true;
    }

    MallocMetaData* current;
    if (size >= _get_block_size(MAX_DEG)) {
        void* p = mmap(nullptr, size + sizeof(MallocMetaData),
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);
        if (p == MAP_FAILED) {
            return nullptr;
        }

        current = (MallocMetaData*)p;
        current->is_mmap = true;
        current->next= current->prev = nullptr;
    }else{
        int d = 0;
        while(_get_block_size(d) < size + sizeof(MallocMetaData)) {
            d++;
        }

        int D = d;
        while(D <= MAX_DEG && data_arr[D] == nullptr) {
            D++;
        }
        if(D>MAX_DEG) return nullptr;
        current = data_arr[D];
        if (D == d) {
            _remove_block_from_arr(current);
        }
        else {
            while (D > d) {
                splitBuddies(current);
                D--;
            }
        }
    }

    active_blocks_num++;
    bytes_allocated += size;
    current->size = size;
    current->is_free = false;
    return (void*)((char*)current + sizeof(MallocMetaData));
}

void* scalloc(size_t num, size_t size) {

    if (num == 0 || size == 0) return nullptr;

    if (num > __SIZE_MAX__ / size) return nullptr;

    if (num * size > 100000000) return nullptr;

    size_t total_size = num * size;

    void* ptr = smalloc(total_size);
    if (!ptr) return nullptr;

    memset(ptr, 0, total_size);

    return ptr;
}

void sfree(void* p) {
    if (!p) return;

    MallocMetaData* block = (MallocMetaData*)((char*)p - sizeof(MallocMetaData));

    if (block->is_free) return;

    if (block->is_mmap){
        active_blocks_num--;
        bytes_allocated -= block->size;
        munmap((void*)block, block->size);
        return;
    }
    else{
        block->is_free = true;
        uniteFreeBuddies(block);
    }
    active_blocks_num--;
    bytes_allocated -= block->size;
}

void* srealloc(void* oldp, size_t size) {

    if(size == 0 || size > 100000000) return nullptr;

    size_t old_payload;
    if(oldp){
        MallocMetaData* old_m = (MallocMetaData*) ((char*)oldp - sizeof(MallocMetaData));
        size_t old_block_size = _get_block_size(old_m->degree);
        old_payload = old_block_size - sizeof(MallocMetaData);
        if(size <= old_payload) return oldp;
    }
    char* new_data = (char*)smalloc(size);
    if(!new_data) return nullptr;

    if(oldp)
        std::memmove(new_data, oldp, std::min(old_payload, size));

    sfree(oldp);
    return new_data;

}

size_t _num_free_blocks() {
    size_t count = 0;
    for(int i = 0; i <= MAX_DEG; i++) {
        MallocMetaData* current = data_arr[i];
        while(current) {
            count++;
            current = current->next;
        }
    }
    return count;
}

size_t _num_free_bytes() {
    size_t count = 0;
    for (int i = 0; i <= MAX_DEG; i++) {
        MallocMetaData* curr = data_arr[i];
        while (curr) {
            count += _get_block_size(curr->degree) - sizeof(MallocMetaData);
            curr = curr->next;
        }
    }
    return count;
}

size_t _num_allocated_blocks() {
    return active_blocks_num + _num_free_blocks();
}

size_t _num_allocated_bytes() {
    return bytes_allocated + _num_free_bytes();
}

size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * _size_meta_data();
}



