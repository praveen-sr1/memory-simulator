# Memory Simulator

A high-performance C++ simulator that demonstrates **Physical Memory Allocation** (Heap) and **Virtual Memory Management** (Paging & Caching). 

This project simulates how an Operating System manages memory, including **First-Fit/Best-Fit/Worst-Fit strategies**, **Memory Coalescing**, **L1/L2 Cache Hierarchies**, and **Page Eviction (LRU)**.

### Features

### 1. Physical Memory Allocator (Continuous)
- **Strategies:** Supports First Fit, Best Fit, and Worst Fit algorithms.
- **Coalescing:** Automatically merges adjacent free blocks to reduce fragmentation.
- **Splitting:** Splits large blocks to minimize wasted space.

### 2. Virtual Memory System (Discontinuous)
- **Paging:** Simulates Virtual-to-Physical address translation.
- **Multi-Level Cache:** Implements L1 (Direct Mapped/2-Way) and L2 (4-Way Set Associative) caches.
- **Eviction Policy:** Uses LRU (Least Recently Used) to evict pages to disk when RAM is full.
- **Process Isolation:** Supports context switching (multiple PIDs) with protected memory spaces.

---

### How to Clone and Build

### Step 1: Clone the Repository
Open your terminal and run:
```bash
git clone https://github.com/praveen-sr1/memory-simulator.git
cd memory-simulator
