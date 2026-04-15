# Dynamic Memory Allocator

A complete implementation of `malloc`, `free`, and `realloc` in C, built from scratch without using any standard library memory management functions. The allocator uses an explicit free list with boundary-tag coalescing, first-fit placement, and block splitting to balance memory utilization and throughput.

---

## Overview

Dynamic memory allocation is one of the most fundamental and notoriously difficult problems in systems programming. This project implements the full allocator interface — `mm_malloc`, `mm_free`, `mm_realloc`, and `mm_init` — on top of a simulated heap using `mem_sbrk`, with no access to the standard library's memory management routines.

The allocator was evaluated against a trace-driven driver (`mdriver`) that replays real-world allocation patterns and computes a performance index balancing space utilization and throughput against `libc malloc`.

---

## Implementation

### Allocator Design

**Free list structure:** Explicit doubly-linked free list — free blocks maintain explicit `next` and `prev` pointers, allowing O(1) free list traversal rather than scanning the entire heap as an implicit list would require.

**Block layout:** Each block contains a header and footer storing the block size and allocation bit, enabling O(1) boundary-tag coalescing in all four cases (both neighbors free, one free, neither free).

**Placement policy:** First-fit search through the explicit free list — finds the first free block large enough to satisfy the request.

**Splitting:** When a free block is larger than needed, the remainder is split into a new free block and inserted back into the free list, reducing internal fragmentation.

**Coalescing:** Immediate coalescing on `mm_free` — when a block is freed, all adjacent free neighbors are merged into a single larger free block using boundary tags to locate neighbors in O(1).

**Heap extension:** When no suitable free block exists, the heap is extended via `mem_sbrk` and the new space is initialized as a free block.

### Core Functions

| Function | Description |
|---|---|
| `mm_init` | Initializes the heap with prologue and epilogue blocks, sets up the free list |
| `mm_malloc` | Searches free list for a fit, splits if necessary, extends heap if no fit found |
| `mm_free` | Frees a block and immediately coalesces with adjacent free neighbors |
| `mm_realloc` | Resizes an allocated block — handles NULL ptr, zero size, and in-place resize cases |
| `mm_check` | Heap consistency checker — validates free list integrity, coalescing correctness, and pointer validity |

---

## Key Technical Concepts

**Boundary tags** — each block stores its size and allocation status in both a header (at the start) and a footer (at the end). This allows the allocator to find a block's neighbors in O(1) by simple pointer arithmetic, which is what makes constant-time coalescing possible.

**Explicit free list vs. implicit** — an implicit allocator scans every block on the heap to find a free one, making allocation O(n) in the number of total blocks. An explicit free list only traverses free blocks, making allocation O(f) in the number of free blocks — a significant speedup on real-world workloads with many allocated blocks.

**Coalescing cases** — when freeing a block, four cases exist depending on whether the previous and next neighbors are allocated or free. Boundary tags make all four cases O(1) regardless of heap size.

**8-byte alignment** — all returned pointers are aligned to 8-byte boundaries to satisfy the same contract as `libc malloc`, enforced by rounding all allocation sizes up to the nearest multiple of 8.

**Internal vs. external fragmentation** — the allocator balances these competing forces. Splitting reduces internal fragmentation (wasted space inside allocated blocks) while immediate coalescing reduces external fragmentation (free blocks too small to satisfy requests).

---

## Heap Consistency Checker

A `mm_check` function was implemented for debugging that validates heap invariants including:

- Every block in the free list is marked as free
- No contiguous free blocks escaped coalescing
- Every free block appears in the free list
- Free list pointers point to valid free blocks
- No allocated blocks overlap
- All heap block pointers fall within valid heap bounds

---

## Performance

The allocator is evaluated on a performance index `P = 0.6U + 0.4 * min(1, T/T_libc)` where `U` is space utilization and `T` is throughput relative to `libc malloc` (~600 Kops/s baseline).

| Metric | Result |
|---|---|
| Performance Index | ~90% |
| Space Utilization | ~72% |
| Throughput | ~14,043 Kops/s |

*Evaluated against a suite of trace files replaying real-world malloc/free/realloc patterns using the `mdriver` trace-driven evaluation harness.*

---

## Build & Run

```bash
# Build
make

# Run full evaluation (verbose)
./mdriver -V

# Run against a specific trace file
./mdriver -f traces/short1-bal.rep

# Run and compare against libc malloc
./mdriver -V -l

# Run grading script
python3 grade-malloc.py
```

---

## Project Structure

```
.
├── mm.c              # Allocator implementation — all core logic
├── mm.h              # Allocator interface declarations
├── memlib.c / memlib.h   # Simulated heap memory system (mem_sbrk, mem_heap_lo, etc.)
├── mdriver.c         # Trace-driven evaluation driver
├── traces/           # Allocation trace files for testing and grading
└── Makefile
```

---

## Skills Demonstrated

- Systems programming in C with extensive pointer arithmetic
- Dynamic memory management: allocation, freeing, coalescing, splitting
- Data structure design: explicit doubly-linked free list with boundary tags
- Heap consistency checking and invariant validation
- Performance profiling and optimization — balancing utilization vs. throughput
- Debugging memory corruption and pointer errors with GDB and Valgrind
- Low-level reasoning about memory layout and alignment
