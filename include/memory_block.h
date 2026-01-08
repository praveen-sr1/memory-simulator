#ifndef MEMORY_BLOCK_H
#define MEMORY_BLOCK_H

#include <cstddef> // for size_t

struct Block {
    int id;             // Unique ID for the block (0 if free)
    size_t size;        // The actual total capacity of this block
    size_t data_size;   
    bool is_free;      
    Block* next;
    Block* prev;
    
    void* get_data() {
        return (void*)(this + 1);
    }
};

#endif