#include <cstring>
#include <iterator>
#include <unistd.h>

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};


// head and tail are dummies and should not be counted in stats and are not part of the heap.


MallocMetadata tail = {
    0, false, nullptr, nullptr
};

MallocMetadata head = {
    0, false, &tail, nullptr
};



void* smalloc(size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }

    MallocMetadata *current = &head;
    while (current->next != &tail && !(current->is_free == true && current->size >= size)) {
        current = current->next;
    }
    if (current->is_free == true && current->size >= size) {
        // means we don't need to allocate new space
        current->is_free = false;
        return (char *)current+sizeof(MallocMetadata);
    }
    void* ptr = sbrk(size+sizeof(MallocMetadata));
    if (ptr == (void*)(-1)) {
        return NULL;
    }
    MallocMetadata* ptr_metadata = (MallocMetadata*) ptr;
    // define curr next to be ptr
    current->next->prev = ptr_metadata;
    current->next = ptr_metadata;
    // define size of malloc metadata
    ptr_metadata->size = size;
    // define is_free of malloc metadata
    ptr_metadata->is_free = false;
    // define next of malloc metadata
    ptr_metadata->next = &tail;
    // define prev of malloc metadata
    ptr_metadata->prev = current;
    ptr = (char *)ptr + sizeof(MallocMetadata);
    // ptr points after the metadata, to the actual space that the user will use
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

void sfree(void* p) {
    if (p == nullptr) {
        return;
    }
    // moving p to the location of is_free
    char* metadata_old_p_addr = (char *)p - sizeof(MallocMetadata);
    MallocMetadata* metadata_old_p = (MallocMetadata *)metadata_old_p_addr;
    metadata_old_p->is_free = true;


}

void* srealloc(void* oldp, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    if (oldp == nullptr) {
        return smalloc(size);
    }
    char* metadata_old_p_addr = (char *)oldp - sizeof(MallocMetadata);
    MallocMetadata* metadata_old_p = (MallocMetadata *)metadata_old_p_addr;
    if (size <= metadata_old_p->size) {
        return oldp;
    }
    void* newp = smalloc(size);
    if (newp == nullptr) {
        return NULL;
    }
    // copy old data to new memory allocation
    memmove(newp, oldp, metadata_old_p->size);
    // free old memory allocation
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
        current = current->next;
    }
    return allocated_blocks;
}

size_t _num_allocated_bytes() {
    size_t allocated_bytes = 0;
    MallocMetadata* current = &head;
    while (current->next != nullptr) {
        allocated_bytes += current->size;
        current = current->next;
    }
    return allocated_bytes;
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * _size_meta_data();
}

