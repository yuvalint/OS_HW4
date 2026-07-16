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
    // define curr next to be ptr
    current->next = (MallocMetadata*)ptr;
    // define size of malloc metadata
    *(size_t *)ptr = size;
    ptr = (char *)ptr + sizeof(size_t);
    // define is_free of malloc metadata
    *(bool *)ptr = false;
    // add 7 bytes for padding
    ptr = (char *)ptr + sizeof(bool) + 7;
    // define next of malloc metadata
    *(MallocMetadata* *)ptr = &tail;
    ptr = (char *)ptr + sizeof(MallocMetadata*);
    // define prev of malloc metadata
    *(MallocMetadata* *)ptr = current;
    ptr = (char *)ptr + sizeof(MallocMetadata*);
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
    char* metadata_old_p = (char *)p - sizeof(MallocMetadata);
    metadata_old_p = metadata_old_p + sizeof(size_t);
    *(bool *)metadata_old_p = true;


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

