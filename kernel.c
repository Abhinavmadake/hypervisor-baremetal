// By explicitly forcing ALL strings into the __TEXT,__text section, we guarantee they 
// are placed inline with the executable code and are safely extracted into the flat binary!

__attribute__((section("__TEXT,__text"))) const char msg_welcome[] = "\r\n======================================================\n   Welcome to Bare-Metal OS! Interactive Shell Live.  \n======================================================\nType 'help' to see available commands.\n\n";
__attribute__((section("__TEXT,__text"))) const char msg_prompt[] = "root@hv-guest:~# ";
__attribute__((section("__TEXT,__text"))) const char msg_help[] = "help";
__attribute__((section("__TEXT,__text"))) const char msg_help_menu[] = "Available commands:\r\n  help      - Show this menu\r\n  meminfo   - Show physical memory layout\r\n  echo      - Print text (e.g., echo hello)\r\n  malloc    - Dynamically allocate memory\r\n  run A     - Start Task A in background\r\n  run B     - Start Task B in background\r\n  tasks     - List running tasks\r\n  kill <id> - Terminate a task\r\n  clear     - Clear screen\r\n  halt      - Shut down the Virtual Machine\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_meminfo[] = "meminfo";
__attribute__((section("__TEXT,__text"))) const char msg_meminfo_out[] = "System Memory (Physical):\r\n  Base Address : 0x80000000\r\n  Heap Start   : 0x80010000\r\n  Heap Current : ";
__attribute__((section("__TEXT,__text"))) const char msg_heap_lim[] = "\r\n  Heap Limit   : ";
__attribute__((section("__TEXT,__text"))) const char msg_malloc[] = "malloc";
__attribute__((section("__TEXT,__text"))) const char msg_malloc_out[] = "Allocated 1024 bytes at: ";
__attribute__((section("__TEXT,__text"))) const char msg_halt[] = "halt";
__attribute__((section("__TEXT,__text"))) const char msg_halt_out[] = "System halted. Goodbye!\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_runA[] = "run A";
__attribute__((section("__TEXT,__text"))) const char msg_runB[] = "run B";
__attribute__((section("__TEXT,__text"))) const char msg_runA_out[] = "Started Task A (ID: ";
__attribute__((section("__TEXT,__text"))) const char msg_runB_out[] = "Started Task B (ID: ";
__attribute__((section("__TEXT,__text"))) const char msg_run_end[] = ")\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_taskA_run[] = " [Task A running] \r\n";
__attribute__((section("__TEXT,__text"))) const char msg_taskB_run[] = " [Task B running] \r\n";
__attribute__((section("__TEXT,__text"))) const char msg_task_entry1[] = "  [";
__attribute__((section("__TEXT,__text"))) const char msg_task_entry2[] = "] Task ";
__attribute__((section("__TEXT,__text"))) const char msg_invalid_kill[] = "Invalid task ID.\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_mmu[] = "mmu";
__attribute__((section("__TEXT,__text"))) const char msg_mmu_out[] = "Initializing AArch64 MMU (39-bit VA, 4KB Granule)...\r\nMMU Enabled! Running with Virtual Memory & Caches ON!\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_tasks[] = "tasks";
__attribute__((section("__TEXT,__text"))) const char msg_tasks_out[] = "Active Tasks:\r\n  [0] Shell (Interactive)\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_task_entry[] = "  [%d] Task %c\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_kill[] = "kill ";
__attribute__((section("__TEXT,__text"))) const char msg_kill_out[] = "Killed task.\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_load_cmd[] = "load ";
__attribute__((section("__TEXT,__text"))) const char msg_load_start[] = "Started Load Task (ID: ";
__attribute__((section("__TEXT,__text"))) const char msg_clear[] = "clear";
__attribute__((section("__TEXT,__text"))) const char msg_clear_cmd[] = "\033[2J\033[H";
__attribute__((section("__TEXT,__text"))) const char msg_taskA_char[] = "A";
__attribute__((section("__TEXT,__text"))) const char msg_taskB_char[] = "B";
__attribute__((section("__TEXT,__text"))) const char msg_bash1[] = "-bash: ";
__attribute__((section("__TEXT,__text"))) const char msg_bash2[] = ": command not found\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_crlf[] = "\r\n";
__attribute__((section("__TEXT,__text"))) const char msg_backspace[] = "\b \b";
__attribute__((section("__TEXT,__text"))) const char msg_0x[] = "0x";
__attribute__((section("__TEXT,__text"))) const char msg_0[] = "0";
__attribute__((section("__TEXT,__text"))) const char msg_load_msg1[] = " [Load Task ID ";
__attribute__((section("__TEXT,__text"))) const char msg_load_msg2[] = " running at ";
__attribute__((section("__TEXT,__text"))) const char msg_load_msg3[] = "% CPU, Memory Consumed: ";
__attribute__((section("__TEXT,__text"))) const char msg_load_msg4[] = " MB] \r\n";
__attribute__((section("__TEXT,__text"))) const char msg_load_char[] = "L";

void yield();

void uart_puts(const char *str) {
    volatile char *uart = (volatile char *)0x100000000;
    while (*str) {
        *uart = *str++;
    }
}

void sleep_ticks(unsigned long long ticks);

void uart_putc(char c) {
    volatile unsigned int *uart_data = (volatile unsigned int *)0x100000000ULL;
    uart_data[0] = c;
}

char uart_getc() {
    volatile unsigned int *uart_data = (volatile unsigned int *)0x100000000ULL;
    while (1) {
        char c = uart_data[0];
        if (c != 0) return c;
        sleep_ticks(1);
    }
}

void gets(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = uart_getc();
        if (c == 0) continue; // Ignore NULL bytes from socket padding!
        if (c == '\r' || c == '\n') {
            uart_puts(msg_crlf);
            break;
        } else if (c == 127 || c == '\b') {
            if (i > 0) {
                i--;
                uart_puts(msg_backspace);
            }
        } else {
            volatile char *uart = (volatile char *)0x100000000;
            *uart = c;
            buf[i++] = c;
        }
    }
    buf[i] = '\0';
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, unsigned long n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int atoi(const char *str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

void uart_puthex(unsigned long long val) {
    uart_puts(msg_0x);
    if (val == 0) { uart_puts(msg_0); return; }
    char buf[20];
    int i = 18;
    buf[19] = '\0';
    while (val > 0) {
        int digit = val % 16;
        if (digit < 10) buf[i--] = '0' + digit;
        else buf[i--] = 'A' + (digit - 10);
        val /= 16;
    }
    uart_puts(&buf[i+1]);
}

void uart_put_num(unsigned int val) {
    if (val == 0) { uart_puts(msg_0); return; }
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    while (val > 0) {
        buf[i--] = '0' + (val % 10);
        val /= 10;
    }
    uart_puts(&buf[i+1]);
}

unsigned long long heap_start;
unsigned long long heap_current;
unsigned long long heap_end;

void report_ram_usage() {
    unsigned long long used = heap_current - heap_start;
    __asm__ volatile("mov x0, %0\n hvc #5" : : "r"(used) : "x0");
}

void init_heap(unsigned long long phys_base, unsigned long long ram_size) {
    heap_start = phys_base + 262144;
    heap_current = heap_start; 
    heap_end = phys_base + ram_size - 65536; 
    report_ram_usage();
}
void *memalign(unsigned long alignment, unsigned long size) {
    if (heap_current % alignment != 0) {
        heap_current = heap_current + (alignment - (heap_current % alignment));
    }
    if (heap_current + size > heap_end) return 0;
    void *ptr = (void *)heap_current;
    heap_current += size;
    report_ram_usage();
    return ptr;
}

void init_mmu() {
    // 1. Set MAIR_EL1 (Index 0: Normal, Index 1: Device-nGnRnE)
    unsigned long long mair = (0x00ULL << 8) | (0xFFULL << 0);
    __asm__ volatile("msr mair_el1, %0" : : "r"(mair));

    // 2. Set TCR_EL1
    unsigned long long tcr = (25ULL << 0) |  // T0SZ = 25 (39-bit VA for TTBR0, starts at Level 1)
                             (0ULL << 14) |  // TG0 = 4KB
                             (3ULL << 12) |  // SH0 = Inner Shareable
                             (0ULL << 10) |  // ORGN0 = Normal memory, Non-Cacheable
                             (0ULL << 8)  |  // IRGN0 = Normal memory, Non-Cacheable
                             (5ULL << 32);   // IPS = 5 (48-bit PA)
    __asm__ volatile("msr tcr_el1, %0" : : "r"(tcr));

    // 3. Initialize TTBR0_EL1 to point to our Page Table
    unsigned long long *l1_table = (unsigned long long *)heap_current;
    heap_current += 4096; // 1 page for L1
    
    // Clear the table
    for(int i=0; i<512; i++) l1_table[i] = 0;
    
    // Identity map kernel memory (256MB)
    // AttrIndx=0 (Normal), AF=1, SH=3, Type=1 (Block)
    l1_table[2] = 0x80000000ULL | (1ULL << 10) | (3ULL << 8) | 1ULL;
    
    // Identity map UART (4GB - 5GB) -> physical 0x100000000
    // AttrIndx=0 (Normal), AF=1, SH=0, Type=1 (Block)
    l1_table[4] = 0x100000000ULL | (1ULL << 10) | (0ULL << 2) | 1ULL;
    
    // 4. Set TTBR0_EL1
    __asm__ volatile("msr ttbr0_el1, %0" : : "r"((unsigned long long)l1_table));
    
    // 5. Ensure TLBs are clear and instructions complete
    __asm__ volatile("tlbi vmalle1\n dsb sy\n isb");
    
    // 6. Enable MMU and Caches in SCTLR_EL1
    unsigned long long sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr &= ~(1ULL << 19); // Clear WXN (Write implies eXecute Never)
    sctlr |= 1;           // M (MMU Enable)
    sctlr |= (1 << 2);    // C (Data Cache Enable)
    sctlr |= (1 << 12);   // I (Instruction Cache Enable)
    __asm__ volatile("msr sctlr_el1, %0" : : "r"(sctlr));
    __asm__ volatile("isb");
    
    // Protect page tables by advancing heap_start to current!
    heap_start = heap_current;
}

void yield() {
    __asm__ volatile("svc #0");
}

extern int num_tasks;
extern volatile unsigned long long system_ticks;
unsigned long long schedule(unsigned long long current_sp);

unsigned long long handle_sync(unsigned long long current_sp) {
    unsigned long long esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    if ((esr >> 26) != 0x15) { // If not SVC
        __asm__ volatile("mov x0, %0\nhvc #42" : : "r"(esr) : "x0");
    }
    if (num_tasks > 1) {
        return schedule(current_sp);
    }
    return current_sp;
}

unsigned long long handle_irq(unsigned long long current_sp) {
    // Acknowledge timer IRQ
    asm volatile("mov x9, 0x1234\nmov x0, 0\nhvc #3" ::: "x0", "x9");
    system_ticks++;
    extern unsigned long long task_sleep_ticks[];
    extern int task_active[];
    for (int i=0; i<8; i++) {
        if (task_active[i] && task_sleep_ticks[i] > 0) {
            task_sleep_ticks[i]--;
        }
    }
    return schedule(current_sp);
}

void *malloc(unsigned long size) {
    if (size % 8 != 0) size = size + (8 - (size % 8));
    if (heap_current + size > heap_end) return 0; 
    void *ptr = (void *)heap_current;
    heap_current += size;
    report_ram_usage();
    return ptr;
}

void *memcpy(void *dest, const void *src, unsigned long n) {
    char *d = (char*)dest; const char *s = (const char*)src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

#define MAX_TASKS 8
unsigned long long task_stacks[MAX_TASKS][1024] __attribute__((aligned(16))); // 8KB per task
unsigned long long task_sps[MAX_TASKS];
int task_active[MAX_TASKS];
int task_types[MAX_TASKS]; // 0=Shell, 1=TaskA, 2=TaskB, 3=LoadTask
unsigned long long task_sleep_ticks[MAX_TASKS]; // 0 = ready, >0 = sleeping
int task_load_cpu[MAX_TASKS];
int task_load_mem[MAX_TASKS];
int current_task = 0; // Starts with 0 (Shell)
int num_tasks = 1;
volatile unsigned long long system_ticks = 0;

void sleep_ticks(unsigned long long ticks) {
    task_sleep_ticks[current_task] = ticks;
    // Yield to let other tasks run while we sleep
    while (task_sleep_ticks[current_task] > 0) {
        yield();
    }
}

void task_a() {
    while (1) {
        uart_puts(msg_taskA_run);
        sleep_ticks(15); // Sleep for ~1.5 seconds (15 * 100ms)
    }
}

void task_b() {
    while (1) {
        uart_puts(msg_taskB_run);
        sleep_ticks(25); // Sleep for ~2.5 seconds (25 * 100ms)
    }
}

void task_load_func() {
    int id = current_task;
    int cpu_load = task_load_cpu[id];
    int mem_mb = task_load_mem[id];
    
    // Consume memory once at startup
    unsigned long bytes_to_alloc = (unsigned long)mem_mb * 1024 * 1024;
    if (bytes_to_alloc > 0) {
        void *ptr = malloc(bytes_to_alloc);
        if (ptr) {
            // Write to memory to fault it in physically
            for (unsigned long i = 0; i < bytes_to_alloc; i += 4096) {
                ((volatile char*)ptr)[i] = 0xFF;
            }
        }
    }
    
    // Calculate CPU sleep/work ratio
    int work_ticks = cpu_load / 10;
    int sleep_t = 10 - work_ticks;
    if (work_ticks <= 0) { work_ticks = 1; sleep_t = 20; }
    
    
    
    while(1) {
        unsigned long long start = system_ticks;
        unsigned long long last_tick = start;
        // Busy spin to consume CPU time slices
        while (system_ticks - start < work_ticks) {
            volatile int x = 0;
            for (int i=0; i<1000; i++) x += i;
        }
        
        uart_puts(msg_load_msg1);
        uart_put_num(id);
        uart_puts(msg_load_msg2);
        uart_put_num(cpu_load);
        uart_puts(msg_load_msg3);
        uart_put_num(mem_mb);
        uart_puts(msg_load_msg4);
        
        if (sleep_t > 0) {
            sleep_ticks(sleep_t);
        }
    }
}

unsigned long long schedule(unsigned long long current_sp) {
    task_sps[current_task] = current_sp;
    
    int next_task = current_task;
    int found = 0;
    
    // First, try to find any ready task OTHER than Task 7 (Idle)
    for (int i=0; i<MAX_TASKS; i++) {
        next_task = (next_task + 1) % MAX_TASKS;
        if (next_task != 7 && task_active[next_task] && task_sleep_ticks[next_task] == 0) {
            found = 1;
            break;
        }
    }
    
    // If no other task is ready, fallback to Idle Task
    if (!found) {
        next_task = 7;
    }
    
    current_task = next_task;
    return task_sps[current_task];
}


void init_task(int task_id, void (*entry_point)(), int type) {
    unsigned long long *sp = &task_stacks[task_id][1024];
    sp -= 34; // 34 slots for context (272 bytes)
    
    for(int i=0; i<34; i++) sp[i] = 0;
    
    sp[30] = (unsigned long long)entry_point; // ELR_EL1
    sp[31] = 0x345; // SPSR_EL1 (EL1h, IRQ unmasked, others masked)
    
    task_sps[task_id] = (unsigned long long)sp;
    task_active[task_id] = 1;
    task_types[task_id] = type;
    num_tasks++;
}

extern void enable_interrupts();
extern void jump_to_task(unsigned long long sp);
void idle_task_func();

void kmain(unsigned long long core_id, unsigned long long ram_size) {
    init_heap(0x80000000, ram_size);
    init_mmu();

    for (int i=0; i<MAX_TASKS; i++) {
        task_active[i] = 0;
        task_sleep_ticks[i] = 0;
    }
    task_active[0] = 1; // Shell is always active
    task_types[0] = 0;
    
    init_task(7, idle_task_func, 4); // Idle Task
    
    uart_puts(msg_clear_cmd);
    uart_puts(msg_welcome);
    
    // Tell host to start timer IRQs! We can take them safely now.
    __asm__ volatile("hvc #4"); 
    enable_interrupts();
    
    char cmd[128];
    while (1) {
        uart_puts(msg_prompt);
        gets(cmd, 128);
        
        if (cmd[0] == '\0') continue;
        
        if (strcmp(cmd, msg_help) == 0) {
            uart_puts(msg_help_menu);
        } 
        else if (strcmp(cmd, msg_meminfo) == 0) {
            uart_puts(msg_meminfo_out);
            uart_puthex(heap_current);
            uart_puts(msg_heap_lim);
            uart_puthex(heap_end);
            uart_puts(msg_crlf);
        } 
        else if (strcmp(cmd, msg_malloc) == 0) {
            void *ptr = malloc(1024);
            uart_puts(msg_malloc_out);
            uart_puthex((unsigned long long)ptr);
            uart_puts(msg_crlf);
        }
        else if (strcmp(cmd, msg_halt) == 0) {
            uart_puts(msg_halt_out);
            __asm__ volatile("hvc #1");
        }
        else if (strcmp(cmd, "debug") == 0) {
            for(int i=0; i<4; i++) {
                uart_puts("Task ");
                uart_put_num(i);
                uart_puts(" SP: 0x");
                uart_puthex(task_sps[i]);
                uart_puts("\r\n");
            }
        }
        else if (strcmp(cmd, msg_mmu) == 0) {
            uart_puts(msg_mmu_out);
            init_mmu();
        }
        else if (strcmp(cmd, msg_clear) == 0) {
            uart_puts(msg_clear_cmd);
        }
        else if (strcmp(cmd, msg_tasks) == 0) {
            unsigned long long nt = num_tasks;
            asm volatile ("mov x0, %0\n\thvc #5" : : "r"(nt) : "x0");
            uart_puts("Active Tasks:\r\n");
            for (int i = 1; i < MAX_TASKS; i++) {
                if (task_active[i] && task_types[i] != 4) {
                    uart_puts(msg_task_entry1);
                    uart_put_num(i);
                    uart_puts(msg_task_entry2);
                    if (task_types[i] == 1) uart_puts(msg_taskA_char);
                    else if (task_types[i] == 2) uart_puts(msg_taskB_char);
                    else if (task_types[i] == 3) uart_puts(msg_load_char);
                    uart_puts(msg_crlf);
                }
            }
        }
        else if (strcmp(cmd, msg_runA) == 0) {
            for (int i = 1; i < MAX_TASKS; i++) {
                if (!task_active[i]) {
                    init_task(i, task_a, 1);
                    uart_puts(msg_runA_out);
                    uart_put_num(i);
                    uart_puts(msg_run_end);
                    break;
                }
            }
        }
        else if (strcmp(cmd, msg_runB) == 0) {
            for (int i = 1; i < MAX_TASKS; i++) {
                if (!task_active[i]) {
                    init_task(i, task_b, 2);
                    uart_puts(msg_runB_out);
                    uart_put_num(i);
                    uart_puts(msg_run_end);
                    break;
                }
            }
        }
        else if (strncmp(cmd, msg_kill, 5) == 0) {
            int target = atoi(&cmd[5]);
            if (target > 0 && target < MAX_TASKS && task_active[target]) {
                task_active[target] = 0;
                num_tasks--;
                uart_puts(msg_kill_out);
                
                if (num_tasks == 1) {
                    heap_current = heap_start;
                }
                
                // Report updated RAM usage
                report_ram_usage();
            } else {
                uart_puts(msg_invalid_kill);
            }
        }
        else if (strncmp(cmd, msg_load_cmd, 5) == 0) {
            int cpu = 0;
            int mem = 0;
            int idx = 5;
            while (cmd[idx] >= '0' && cmd[idx] <= '9') {
                cpu = cpu * 10 + (cmd[idx] - '0');
                idx++;
            }
            if (cmd[idx] == ' ') idx++;
            while (cmd[idx] >= '0' && cmd[idx] <= '9') {
                mem = mem * 10 + (cmd[idx] - '0');
                idx++;
            }
            if (cpu > 100) cpu = 100;
            
            for (int t = 1; t < MAX_TASKS; t++) {
                if (!task_active[t]) {
                    task_load_cpu[t] = cpu;
                    task_load_mem[t] = mem;
                    init_task(t, task_load_func, 3);
                    uart_puts(msg_load_start);
                    uart_put_num(t);
                    uart_puts(msg_run_end);
                    break;
                }
            }
        }
        else if (strncmp(cmd, "echo ", 5) == 0) {
            uart_puts(&cmd[5]);
            uart_puts(msg_crlf);
        }
        else {
            uart_puts(msg_bash1);
            uart_puts(cmd);
            uart_puts(msg_bash2);
        }
    }
}
void idle_task_func() {
    while (1) {
        __asm__ volatile("hvc #99");
    }
}
