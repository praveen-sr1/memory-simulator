#include <iostream>
#include <cstdlib>
#include <vector>
#include <iomanip> 
#include <algorithm>
#include "memory_block.h"

// --- GLOBALS ---
void* memory_start = nullptr;
size_t total_memory_size = 0;
Block* free_list_head = nullptr;

// Statistics Counters
int next_block_id = 1;
int total_allocations = 0;
int failed_allocations = 0;

// Strategy Enum
enum AllocatorType { FIRST_FIT, BEST_FIT, WORST_FIT };
AllocatorType current_allocator = FIRST_FIT;

// Helper: Relative Address
size_t get_relative_addr(void* ptr) {
    return (char*)ptr - (char*)memory_start;
}

// --- INITIALIZATION ---
void init_memory(size_t size) {
    if (memory_start != nullptr) free(memory_start);

    memory_start = std::malloc(size);
    total_memory_size = size;

    free_list_head = (Block*)memory_start;
    free_list_head->size = size - sizeof(Block);
    free_list_head->data_size = 0; 
    free_list_head->is_free = true;
    free_list_head->id = 0;
    free_list_head->next = nullptr;
    free_list_head->prev = nullptr;

    next_block_id = 1;
    total_allocations = 0;
    failed_allocations = 0;
    
    std::cout << "Memory initialized " << size << " bytes." << std::endl;
}

void set_allocator(std::string type) {
    if (type == "FIRST_FIT") current_allocator = FIRST_FIT;
    else if (type == "BEST_FIT") current_allocator = BEST_FIT;
    else if (type == "WORST_FIT") current_allocator = WORST_FIT;
    else std::cout << "Unknown allocator. Defaulting to First Fit." << std::endl;
}

// --- STRATEGIES ---
Block* find_first_fit(size_t size) {
    Block* current = free_list_head;
    while (current) {
        if (current->is_free && current->size >= size) return current;
        current = current->next;
    }
    return nullptr;
}

Block* find_best_fit(size_t size) {
    Block* best = nullptr;
    Block* current = free_list_head;
    while (current) {
        if (current->is_free && current->size >= size) {
            if (best == nullptr || current->size < best->size) best = current;
        }
        current = current->next;
    }
    return best;
}

Block* find_worst_fit(size_t size) {
    Block* worst = nullptr;
    Block* current = free_list_head;
    while (current) {
        if (current->is_free && current->size >= size) {
            if (worst == nullptr || current->size > worst->size) worst = current;
        }
        current = current->next;
    }
    return worst;
}

// --- MALLOC ---
void* my_malloc(size_t size) {
    total_allocations++;
    Block* target = nullptr;
    
    if (current_allocator == FIRST_FIT) target = find_first_fit(size);
    else if (current_allocator == BEST_FIT) target = find_best_fit(size);
    else if (current_allocator == WORST_FIT) target = find_worst_fit(size);

    if (!target) {
        failed_allocations++;
        std::cout << "Allocation failed (OOM)" << std::endl;
        return nullptr;
    }

    // SPLITTING LOGIC
    if (target->size >= size + sizeof(Block) + 1) {
        Block* new_block = (Block*)((char*)target + sizeof(Block) + size);
        
        new_block->size = target->size - size - sizeof(Block);
        new_block->data_size = 0;
        new_block->is_free = true;
        new_block->id = 0;
        new_block->next = target->next;
        new_block->prev = target;

        if (target->next) target->next->prev = new_block;
        target->next = new_block;
        target->size = size;
    }

    // UPDATE METADATA
    target->is_free = false;
    target->id = next_block_id++;
    target->data_size = size; 

    std::cout << "Allocated block id=" << target->id 
              << " at address=0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') 
              << get_relative_addr(target->get_data()) << std::dec << std::endl;

    return target->get_data();
}

// --- FREE ---
void my_free(int id) {
    Block* current = free_list_head;
    while (current) {
        if (!current->is_free && current->id == id) {
            current->is_free = true;
            current->id = 0;
            current->data_size = 0; 
            
            std::cout << "Block " << id << " freed and merged" << std::endl;

            // Coalesce Right
            if (current->next && current->next->is_free) {
                Block* next = current->next;
                current->size += sizeof(Block) + next->size;
                current->next = next->next;
                if (current->next) current->next->prev = current;
            }
            // Coalesce Left
            if (current->prev && current->prev->is_free) {
                Block* prev = current->prev;
                prev->size += sizeof(Block) + current->size;
                prev->next = current->next;
                if (current->next) current->next->prev = prev;
            }
            return;
        }
        current = current->next;
    }
    std::cout << "Error: Block ID " << id << " not found." << std::endl;
}

// --- COMMANDS ---

void dump_memory() {
    Block* current = free_list_head;
    std::cout << "Memory Map:\n";
    while (current) {
        size_t start = get_relative_addr(current->get_data());
        size_t end = start + current->size - 1;
        
        std::cout << "[0x" << std::hex << std::setw(4) << std::setfill('0') << start 
                  << " - 0x" << std::setw(4) << std::setfill('0') << end << "] " << std::dec;
        
        if (current->is_free) {
            std::cout << "FREE (" << current->size << " bytes)";
        } else {
            std::cout << "USED (id=" << current->id << ", size=" << current->size 
                      << ", requested=" << current->data_size << ")";
        }
        std::cout << std::endl;
        current = current->next;
    }
}

void print_stats() {
    size_t total_used_capacity = 0; 
    size_t total_user_data = 0;     
    size_t total_free_memory = 0;
    size_t largest_free_block = 0;
    size_t internal_frag_bytes = 0;
    int free_blocks_count = 0; 

    Block* current = free_list_head;
    while (current) {
        if (current->is_free) {
            total_free_memory += current->size;
            if (current->size > largest_free_block) largest_free_block = current->size;
            free_blocks_count++;
        } else {
            total_used_capacity += current->size;
            total_user_data += current->data_size;
            // Internal Frag = Block Size - Requested Size
            internal_frag_bytes += (current->size - current->data_size);
        }
        current = current->next;
    }

    // --- CALCULATIONS ---

    // 1. External Fragmentation
    double ext_frag_percent = 0.0;
    if (total_free_memory > 0) {
        ext_frag_percent = 100.0 * (1.0 - ((double)largest_free_block / total_free_memory));
    }

    // 2. Memory Utilization
    double utilization = 0.0;
    if (total_memory_size > 0) {
        utilization = 100.0 * ((double)total_user_data / total_memory_size);
    }

    // 3. Allocation Success Rate
    int success_allocs = total_allocations - failed_allocations;
    double success_rate = 0.0;
    if (total_allocations > 0) {
        success_rate = 100.0 * ((double)success_allocs / total_allocations);
    }

    // --- REPORT ---
    std::cout << "\n========== MEMORY METRICS ==========\n";
    std::cout << "  Total Memory:       " << total_memory_size << " bytes\n";
    std::cout << "  Current Free:       " << total_free_memory << " bytes\n";
    std::cout << "------------------------------------\n";
    std::cout << "PERFORMANCE:\n";
    std::cout << "  Success Rate:       " << std::fixed << std::setprecision(2) << success_rate 
              << "% (" << success_allocs << " ok / " << failed_allocations << " failed)\n";
    std::cout << "------------------------------------\n";
    std::cout << "EFFICIENCY:\n";
    std::cout << "  Memory Utilization: " << utilization << "% (Data / Total Capacity)\n";
    std::cout << "  Internal Frag:      " << internal_frag_bytes << " bytes (Wasted inside blocks)\n";
    std::cout << "  External Frag:      " << ext_frag_percent << "% (Wasted between blocks)\n";
    std::cout << "  Fragmentation Holes:" << free_blocks_count << " (Separate free chunks)\n";
    std::cout << "====================================\n";
}