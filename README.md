# Operating Systems (MTL458) — Assignments

Subhojit Ghosal - 2022MT61976

This repository contains my implementations for the Operating Systems course assignments (Process Management — Shell, CPU Scheduling, and Virtual Memory / Dynamic Allocator). Each assignment is self-contained and includes source, usage instructions, and example runs.

---

# Assignment 1 — Custom Shell

## AIM
Build a simple interactive shell in C that mimics basic `bash` behaviour. It parses user input and uses the `exec` family to run external commands.

## Features
- Basic command execution (`ls`, `cat`, `sleep`, etc.)
- Single pipes (`ls | grep pattern`)
- I/O redirection: `<`, `>`, `>>`
- Command separators: `;` and `&&`
- Wildcard expansion (`*.c`)
- Built-ins: `cd`, `history` (stores up to 2048 commands), `exit`

## File
- `A1.c`

## Build & Run
```bash
gcc A1.c -o my_shell
./my_shell
```

Prompt:
```
MTL458 >
```

### Example usage

Wildcards:
```bash
MTL458 > ls *.c
A1.c
```

Piping:
```bash
MTL458 > ls | grep A1
A1.c
```

Redirection:
```bash
MTL458 > echo "Hello OS" > output.txt
MTL458 > cat output.txt
Hello OS
```

Chaining:
```bash
MTL458 > echo first; echo second
first
second
```

History:
```bash
MTL458 > history 3
echo "Hello OS" > output.txt
cat output.txt
echo first; echo second
```

---

# Assignment 2 — CPU Scheduling Policies

## AIM
Implement offline and online CPU scheduling algorithms and print scheduling traces and metrics.

## Implemented Algorithms

### Offline
- **FCFS**
- **Round Robin (RR)**
- **MLFQ**

### Online
- **Adaptive MLFQ**
- **SJF** (Shortest Job First using historical burst prediction)

## Files
- `offline_schedulers.h`
- `online_schedulers.h`
- `main_scheduler_example.c`

## Example Driver (`main_scheduler_example.c`)
```c
#include "offline_schedulers.h"
#include "online_schedulers.h"

int main() {
    // --- Test Offline FCFS ---
    int n = 2;
    Process p[2];

    p[0].command = "sleep 1";
    p[1].command = "ls";

    printf("Running Offline FCFS:\n");
    FCFS(p, n);

    // --- Test Online Scheduling ---
    // ShortestJobFirst(2);  // reads from stdin for dynamic arrivals

    return 0;
}
```

## Build & Run
```bash
gcc main_scheduler_example.c -o scheduler
./scheduler
```

### Online Input Format
Each line of input is treated as a new process arrival.

## Output
Example console trace:
```
sleep 1, 0, 1002
ls, 1002, 1005
```

CSV outputs (examples):
- `result_offline_FCFS.csv`
- `result_online_SJF.csv`

---

# Assignment 3 — Virtual Memory / Dynamic Allocator

## AIM
Implement a custom allocator using `mmap`, managing a free list and supporting five strategies.

## Supported Strategies
1. **First Fit**
2. **Next Fit**
3. **Best Fit**
4. **Worst Fit**
5. **Buddy Allocator** (power-of-two block splitting)

## Files
- `A3.h`

## Example Test (`test_mem.c`)
```c
#include <stdio.h>
#include <string.h>
#include "A3.h"

int main() {
    printf("--- Testing Best Fit ---\n");

    int *arr = (int*) malloc_best_fit(5 * sizeof(int));
    if (arr) {
        printf("Allocated 5 integers at %p\n", (void*)arr);
        for(int i=0; i<5; i++) arr[i] = i * 10;
    }

    char *str = (char*) malloc_best_fit(100);
    if (str) {
        printf("Allocated string buffer at %p\n", (void*)str);
        strcpy(str, "Hello Memory!");
        printf("String content: %s\n", str);
    }

    my_free(arr);
    my_free(str);

    return 0;
}
```

## Build & Run
```bash
gcc test_mem.c -o mem_test
./mem_test
```

## Buddy Allocator Example
```c
void* p1 = malloc_buddy_alloc(30);
void* p2 = malloc_buddy_alloc(5000);
my_free(p1);
my_free(p2);
```

---

# Testing & Validation
- **Shell:** Test pipes, redirects, wildcards, and built-ins directly.
- **Schedulers:** Run offline tests with process lists; check CSVs in `results/`.
- **Allocator:** Use `test_mem.c` and custom tests for fragmentation and behaviour under stress.

---

# Known Limitations / TODO
- Shell supports only **single** pipe.
- Online schedulers rely on stdin; automated input harness can be added.
- Allocator is **not thread-safe**.
- More benchmarks and unit tests can be added.

---

# Author
**Subhojit**  
Dual Degree (Maths & Computing), IIT Delhi — **Class of 2027**

---

# License
MIT License (recommended). Add a `LICENSE` file if required.

