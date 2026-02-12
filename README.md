#  Custom Fixed-Size Memory Allocator (C)

A low-level **manual memory management system** written in C that simulates how operating systems and runtime environments manage heap memory.

This project implements a **fixed-size memory pool allocator** with:

- Block headers and metadata
- First-fit allocation strategy
- Block splitting
- Coalescing on free
- Fragmentation tracking
- Corruption detection using magic cookies and checksums
- Text-based memory visualization
- Allocation statistics reporting

Designed for **systems programming, operating systems, and embedded development learning**.

---

#  Features

## Memory Pool
- Fixed **64 KB** memory pool
- **8-byte alignment** for safe data access
- Custom **block header structure** storing:
  - Magic cookie for corruption detection
  - Block size
  - Free/allocated flag
  - Doubly-linked list pointers
  - Checksum for integrity validation

## Allocation (`my_malloc`)
- **First-fit search** through free list
- **Automatic block splitting** when space allows
- Updates allocator statistics
- Detects corrupted headers before allocation

## Freeing (`my_free`)
- Prevents:
  - Double free
  - Freeing corrupted memory
- **Coalesces adjacent free blocks**
- Restores usable memory to the pool

## Fragmentation Analysis
- Calculates **fragmentation score (%)**
- Tracks:
  - Largest free block
  - Total free space
  - Number of free blocks

## Memory Visualization
- Console-based **memory map display**
- Shows:
  - Allocated blocks (`#`)
  - Free blocks (`.`)
  - Block sizes and address ranges

## Statistics Dashboard
Displays:

- Total pool size
- Allocated bytes
- Free bytes
- Active allocations
- Fragmentation score
- Header size and alignment

---

#  Compilation & Running

## Compile

```bash
gcc -o allocator memory_allocator_fixed.c
