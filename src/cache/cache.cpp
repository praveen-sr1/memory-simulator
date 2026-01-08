#include "../../include/cache.h"
#include <iomanip>
#include <climits>

Cache::Cache(int level, size_t size, size_t block_size, int associativity, 
             Cache* next_level, std::string policy)
    : level(level), size(size), block_size(block_size), 
      associativity(associativity), policy(policy), next_level(next_level) 
{
    num_sets = size / (block_size * associativity);
    sets.resize(num_sets, std::vector<CacheLine>(associativity));
    timer = 0;
    
    std::cout << "[Hardware] L" << level << " Cache Init: " 
              << size/1024 << "KB, " << associativity << "-way, " 
              << policy << "\n";
}

void Cache::access(uint32_t address, bool is_write) {
    timer++;
    
    // 1. Calculate Index and Tag
    uint32_t index = (address / block_size) % num_sets;
    uint32_t tag = address / (block_size * num_sets);

    // 2. Search for the Tag in the Set
    bool hit = false;
    for (int i = 0; i < associativity; i++) {
        if (sets[index][i].valid && sets[index][i].tag == tag) {
            // --- CACHE HIT ---
            hit = true;
            hits++;
            
            // UPDATE POLICY:
            // LRU: Update timestamp on access
            // FIFO: Do NOT update timestamp (only matters when inserted)
            if (policy == "LRU") {
                sets[index][i].last_used_time = timer;
            }
            
            if (is_write) sets[index][i].dirty = true;
            // std::cout << "[L" << level << " HIT] "; // Optional debug
            return; 
        }
    }

    // --- CACHE MISS ---
    misses++;
    std::cout << "[L" << level << " MISS] -> ";

    // 3. Penalty Propagation: Fetch from Next Level (or RAM)
    if (next_level) {
        next_level->access(address, false); // Read from lower memory
    } else {
        std::cout << "[RAM FETCH] ";
    }

    // 4. Store in this Cache (Allocation)
    // Find an empty slot OR Evict
    int empty_slot = -1;
    for (int i = 0; i < associativity; i++) {
        if (!sets[index][i].valid) {
            empty_slot = i;
            break;
        }
    }

    if (empty_slot != -1) {
        // Use empty slot
        sets[index][empty_slot].valid = true;
        sets[index][empty_slot].tag = tag;
        sets[index][empty_slot].dirty = is_write;
        sets[index][empty_slot].inserted_time = timer; // For FIFO
        sets[index][empty_slot].last_used_time = timer; // For LRU
    } else {
        // Set is full -> Needs Eviction
        evict(index, address, is_write);
    }
}

void Cache::evict(int set_index, uint32_t address, bool is_write) {
    int victim_way = -1;
    uint32_t min_time = UINT_MAX;

    // 5. Select Victim based on Policy
    for (int i = 0; i < associativity; i++) {
        uint32_t time_metric = (policy == "FIFO") 
                             ? sets[set_index][i].inserted_time 
                             : sets[set_index][i].last_used_time;

        if (time_metric < min_time) {
            min_time = time_metric;
            victim_way = i;
        }
    }

    // 6. Evict
    // (Optional: If dirty, write back to next level)
    // if (sets[set_index][victim_way].dirty && next_level) {
    //    next_level->access(...) 
    // }

    // Replace
    uint32_t tag = address / (block_size * num_sets);
    sets[set_index][victim_way].tag = tag;
    sets[set_index][victim_way].valid = true;
    sets[set_index][victim_way].dirty = is_write;
    sets[set_index][victim_way].inserted_time = timer;   // Reset for FIFO
    sets[set_index][victim_way].last_used_time = timer;  // Reset for LRU
}

void Cache::print_stats() {
    std::cout << "--- L" << level << " Cache Stats (" << policy << ") ---\n";
    std::cout << "Hits: " << hits << " | Misses: " << misses << "\n";
    double ratio = (hits + misses > 0) ? 100.0 * hits / (hits + misses) : 0.0;
    std::cout << "Hit Ratio: " << std::fixed << std::setprecision(2) << ratio << "%\n";
}