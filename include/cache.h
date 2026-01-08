#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>

struct CacheLine {
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    
    uint32_t last_used_time = 0; // For LRU
    uint32_t inserted_time = 0;  // For FIFO
};

class Cache {
public:
    // Configurable: Size, Block Size, Associativity, Next Level, Policy
    Cache(int level, size_t size, size_t block_size, int associativity, 
          Cache* next_level, std::string policy = "LRU");

    void access(uint32_t address, bool is_write);
    void print_stats();

private:
    int level;
    size_t size;
    size_t block_size;
    int associativity;
    std::string policy; // "FIFO" or "LRU"
    Cache* next_level;

    int num_sets;
    uint32_t timer; // Global timer for this cache

    // The Cache Storage: A Vector of Sets
    std::vector<std::vector<CacheLine>> sets;

    int hits = 0;
    int misses = 0;

    void evict(int set_index, uint32_t address, bool is_write);
};

#endif