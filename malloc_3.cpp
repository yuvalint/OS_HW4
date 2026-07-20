#include <cstring>
#include <iterator>
#include <unistd.h>

#define MAX_ORDER 10
#define TOTAL_MEMORY_BLOCK 131072
#define MEMORY_BLOCKS 32

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};


bool is_initialized = false;

MallocMetadata arr[MAX_ORDER+1];


// head and tail are dummies and should not be counted in stats and are not part of the heap.



MallocMetadata dummies[MAX_ORDER+1];



void initializeArray(MallocMetadata arr[MAX_ORDER+1]) {
    // initialize dummies
    for (int i = 0; i < MAX_ORDER + 1; ++i) {
       MallocMetadata head = {
            0, false, nullptr, nullptr};
        dummies[i] = head;
        arr[i] = dummies[i];
    }
    // find the current sbrk
    void* current_sbrk = sbrk(0);
    uintptr_t alignment  = (uintptr_t)current_sbrk % (TOTAL_MEMORY_BLOCK * MEMORY_BLOCKS);
    void* ptr = sbrk(TOTAL_MEMORY_BLOCK*(MEMORY_BLOCKS+1) - alignment);
    ptr = (char *)ptr + alignment;
    for (int i=0; i < MEMORY_BLOCKS; ++i) {
        MallocMetadata* current = (MallocMetadata*)ptr;
        current->size = TOTAL_MEMORY_BLOCK;
        current->is_free = true;
        void* new_ptr = (char *)ptr + TOTAL_MEMORY_BLOCK;
        if (i == MEMORY_BLOCKS - 1) {
            current->next = nullptr;
        } else {
            current->next = (MallocMetadata *)new_ptr;
        }
        void* old_ptr = (char *)ptr - TOTAL_MEMORY_BLOCK;
        if (i==0) {
            current->prev = &arr[MAX_ORDER];
            arr[MAX_ORDER].next = current;
        } else {
            current->prev = (MallocMetadata *)old_ptr;
        }
        ptr = new_ptr;
    }
    is_initialized = true;



}

int find_order(size_t size) {
    size = size + sizeof(MallocMetadata);
    int index = 0;
    size_t curr_size = 128;
    while (size > curr_size && index < MAX_ORDER) {
        index++;
        curr_size *= 2;
    }
    return index;
}


void* get_memory_block_with_metadata(int order) {
    if (order > MAX_ORDER) {
        return NULL;
    }
    // check if arr is not empty
    if (arr[order].next != nullptr) {
        MallocMetadata* memoryBlock = arr[order].next;
        arr[order].next = memoryBlock->next;
        memoryBlock->next->prev = &arr[order];
        memoryBlock->prev = nullptr;
        return memoryBlock;
    }
    MallocMetadata* memoryBlock = (MallocMetadata *)get_memory_block_with_metadata(order+1);
    if (memoryBlock == nullptr) {
        return NULL;
    }
    memoryBlock->size = memoryBlock->size / 2;
    void* newMemoryBlock = (char *)memoryBlock + memoryBlock->size;
    MallocMetadata* buddy = (MallocMetadata *) newMemoryBlock;
    buddy->size = memoryBlock->size;
    buddy->is_free = true;
    buddy->prev = &arr[order];
    buddy->next = nullptr;
    arr[order].next = buddy;
    return (void *)memoryBlock;
}

void* get_memory_block(int order) {
    void* memoryBlock = get_memory_block_with_metadata(order);
    if (memoryBlock == NULL) {
        return nullptr;
    }
    ((MallocMetadata*)memoryBlock)->is_free = false;
    char* actualBlock = (char *)memoryBlock + sizeof(MallocMetadata);
    return (void *)actualBlock;
}

void* smalloc(size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }
    if (!is_initialized) {
        initializeArray(arr);
    }
    int order = find_order(size);
    void* ptr = get_memory_block(order);
    if (ptr == nullptr) {
        return NULL;
    }
    return ptr;


    //
    // MallocMetadata *current = &head;
    // while (current->next != &tail && !(current->is_free == true && current->size >= size)) {
    //     current = current->next;
    // }
    // if (current->is_free == true && current->size >= size) {
    //     // means we don't need to allocate new space
    //     current->is_free = false;
    //     return (char *)current+sizeof(MallocMetadata);
    // }
    // void* ptr = sbrk(size+sizeof(MallocMetadata));
    // if (ptr == (void*)(-1)) {
    //     return NULL;
    // }
    // // define curr next to be ptr
    // current->next = (MallocMetadata*)ptr;
    // // define size of malloc metadata
    // *(size_t *)ptr = size;
    // ptr = (char *)ptr + sizeof(size_t);
    // // define is_free of malloc metadata
    // *(bool *)ptr = false;
    // // add 7 bytes for padding
    // ptr = (char *)ptr + sizeof(bool) + 7;
    // // define next of malloc metadata
    // *(MallocMetadata* *)ptr = &tail;
    // ptr = (char *)ptr + sizeof(MallocMetadata*);
    // // define prev of malloc metadata
    // *(MallocMetadata* *)ptr = current;
    // ptr = (char *)ptr + sizeof(MallocMetadata*);
    // // ptr points after the metadata, to the actual space that the user will use
    return ptr;

}

void* scalloc(size_t num, size_t size) {
    void* ptr = smalloc(num * size);
    if (ptr == nullptr) {
        return ptr;
    }
    std::memset(ptr, 0, num*size);
    return ptr;

}

MallocMetadata* merge(MallocMetadata* metadata_p, MallocMetadata* metadata_buddy) {
    // remove buddy from its list
    metadata_buddy->prev->next = metadata_buddy->next;
    if (metadata_buddy->next != nullptr) {
        metadata_buddy->next->prev = metadata_buddy->prev;
    }
    metadata_buddy->next = nullptr;
    metadata_buddy->prev = nullptr;
    if ((uintptr_t)metadata_p > (uintptr_t)metadata_buddy) {
        metadata_buddy->size = 2*(metadata_buddy->size);
        return metadata_buddy;
    }
    metadata_p->size = 2*(metadata_p->size);
    return metadata_p;

}

void sfree(void* p) {
    if (p == nullptr) {
        return;
    }
    char* metadata_old_p_addr = (char *)p - sizeof(MallocMetadata);
    MallocMetadata* metadata_old_p = (MallocMetadata*) metadata_old_p_addr;
    void* metadata_buddy_addr = (void *)((uintptr_t)metadata_old_p ^ metadata_old_p->size);
    MallocMetadata* metadata_buddy = (MallocMetadata *) metadata_buddy_addr;
    if (metadata_buddy->is_free && metadata_buddy->size == metadata_old_p->size
        && metadata_old_p->size != TOTAL_MEMORY_BLOCK) {
        // merge p and its buddy
        metadata_old_p = merge(metadata_old_p, metadata_buddy);
        p = (char *)metadata_old_p + sizeof(MallocMetadata);
        return sfree(p);


    } else {
        int order = find_order(metadata_old_p->size);
        MallocMetadata* current = &arr[order];
        while (current->next != nullptr && (uintptr_t)current->next < (uintptr_t)metadata_old_p) {
            current = current->next;
        }
        metadata_old_p->next = current->next;
        current->next = metadata_old_p;
        metadata_old_p->prev = current;
        if (metadata_old_p->next != nullptr) {
            metadata_old_p->next->prev = metadata_old_p;
        }
        metadata_old_p->is_free = true;

    }



}

void* srealloc(void* oldp, size_t size) {
    if (oldp == nullptr) {
        return smalloc(size);
    }
    char* metadata_old_p = (char *)oldp - sizeof(MallocMetadata);
    size_t curr_size = *(size_t *)metadata_old_p;
    if (size <= curr_size) {
        return oldp;
    }
    void* newp = smalloc(size);
    if (newp == nullptr) {
        return NULL;
    }
    sfree(oldp);
    return newp;

}

size_t _num_free_blocks() {
    size_t free_blocks = 0;
    MallocMetadata* current = &head;
    while (current->next != nullptr) {
        if (current->is_free) {
            free_blocks++;
        }
        current = current->next;
    }
    // if we reached here it means that current is tail
    return free_blocks;
}

size_t _num_free_bytes() {
    size_t free_bytes = 0;
    MallocMetadata* current = &head;
    while (current->next != nullptr) {
        if (current->is_free) {
            free_bytes += current->size;
        }
        current = current->next;
    }
    // if we reached here it means that current is tail
    return free_bytes;
}

size_t _num_allocated_blocks() {
    size_t allocated_blocks = 0;
    MallocMetadata* current = &head;
    while (current->next != &tail) {
        allocated_blocks++;
    }
    return allocated_blocks;
}

size_t _num_allocated_bytes() {
    size_t allocated_bytes = 0;
    MallocMetadata* current = &head;
    while (current->next != nullptr) {
        allocated_bytes += current->size;
    }
    return allocated_bytes;
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * _size_meta_data();
}

