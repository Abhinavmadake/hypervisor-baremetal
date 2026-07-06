#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdatomic.h>
#include <time.h>
#include <Hypervisor/Hypervisor.h>

#define SOCKET_PATH "/tmp/hv_daemon.sock"
#define MAX_CLIENTS 64

#if defined(__aarch64__)
#define VM_PHYS_BASE_START 0x80000000
#else
#define VM_PHYS_BASE_START 0x00000000
#endif

uint64_t next_phys_base = VM_PHYS_BASE_START;

// Forward declaration
typedef struct vcpu_thread_arg_t vcpu_thread_arg_t;

void *timer_irq_thread(void *arg);
pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char payload_path[256];
    uint64_t arg0;
    uint64_t arg1;
    uint64_t ram_mb; // NEW: Dynamically requested RAM limit
    int num_cores;
    int client_id;
    int fd;
    
    // Internal Daemon State
    _Atomic int active_cores;
    uint64_t phys_base;
    uint64_t vm_mem_size_bytes; // Precomputed byte size
    void *host_mem;
} client_req_t;

typedef struct vcpu_thread_arg_t {
    client_req_t *req;
    int core_id;
    char rx_char;
    int has_rx;
    int timer_pending;
    hv_vcpu_t vcpu;
    pthread_t timer_tid;
    int timer_active;
    _Atomic int timer_stop;
} vcpu_thread_arg_t;

void *timer_irq_thread(void *arg) {
    vcpu_thread_arg_t *varg = (vcpu_thread_arg_t *)arg;
    while (!varg->timer_stop) {
        usleep(100000); // Fire every 100ms
        if (varg->timer_stop) break;
        varg->timer_pending = 1;
        hv_vcpus_exit(&varg->vcpu, 1);
    }
    return NULL;
}

// Dashboard Status Struct
typedef struct {
    int active;
    int client_id;
    int core_id;
    char payload_name[64];
    uint64_t phys_base;
    uint64_t mem_size_bytes;
    uint64_t used_mem_bytes;
    int exits_handled;
    char status[64];
    uint64_t busy_time_ns;
    uint64_t idle_time_ns;
    float cpu_usage_pct;
} client_stat_t;

client_stat_t active_clients[MAX_CLIENTS];
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

const char* get_basename(const char* path) {
    const char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}

void update_status(int slot, const char* status) {
    pthread_mutex_lock(&stats_mutex);
    strncpy(active_clients[slot].status, status, sizeof(active_clients[slot].status)-1);
    pthread_mutex_unlock(&stats_mutex);
}

void make_bar(float pct, char *buf, int width) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int fill = (int)((pct / 100.0f) * width);
    buf[0] = '[';
    for (int i=0; i<width; i++) {
        if (i < fill) buf[1+i] = '#';
        else buf[1+i] = ' ';
    }
    buf[1+width] = ']';
    buf[2+width] = '\0';
}

void* monitor_thread(void* arg) {
    int tick = 0;
    while (1) {
        pthread_mutex_lock(&stats_mutex);
        
        if (tick % 10 == 0) {
            for (int i=0; i<MAX_CLIENTS; i++) {
                if (active_clients[i].active) {
                    uint64_t true_total_ns = 1000000000ULL; // 1 second interval
                    if (active_clients[i].idle_time_ns > true_total_ns) {
                        active_clients[i].idle_time_ns = true_total_ns;
                    }
                    uint64_t busy_ns = true_total_ns - active_clients[i].idle_time_ns;
                    active_clients[i].cpu_usage_pct = ((float)busy_ns * 100.0f) / (float)true_total_ns;

                    active_clients[i].busy_time_ns = 0;
                    active_clients[i].idle_time_ns = 0;
                }
            }
        }
        tick++;
        
        printf("\033[2J\033[H");
        printf("====================================================================================================\n");
        printf("                                   BARE-METAL HYPERVISOR MONITOR                                    \n");
        printf("====================================================================================================\n");
        
        int active_count = 0;
        uint64_t mem_used = 0;
        float total_cpu_pct = 0.0f;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (active_clients[i].active && strcmp(active_clients[i].status, "Terminated") != 0) {
                active_count++;
                total_cpu_pct += active_clients[i].cpu_usage_pct;
            }
            if (active_clients[i].active && active_clients[i].core_id == 0) mem_used += active_clients[i].mem_size_bytes;
        }
        
        uint64_t mem_total = 8192ULL * 1024ULL * 1024ULL; // 8GB Nominal Pool
        float mem_pct = ((float)mem_used / (float)mem_total) * 100.0f;
        char mem_bar[24]; make_bar(mem_pct, mem_bar, 20);
        
        float host_cpu = active_count > 0 ? (total_cpu_pct / active_count) : 0.0f;
        char cpu_bar[24]; make_bar(host_cpu, cpu_bar, 20);
        
        printf("  Active VCPUs: %-5d       RAM Provisioned: %llu MB / 8192 MB\n", active_count, (unsigned long long)(mem_used / (1024*1024)));
        printf("  Overall CPU: %s %5.1f%%   Overall RAM: %s %5.1f%%\n", cpu_bar, host_cpu, mem_bar, mem_pct);
        printf("-----------------------------------------------------------------------------------------------------------------------\n");
        printf(" Client | Core | Payload          | CPU Usage             | RAM Usage                   | Exits | Status \n");
        printf("-----------------------------------------------------------------------------------------------------------------------\n");
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (active_clients[i].active) {
                char vcpu_bar[16]; make_bar(active_clients[i].cpu_usage_pct, vcpu_bar, 10);
                float vm_ram_pct = active_clients[i].mem_size_bytes > 0 ? ((float)active_clients[i].used_mem_bytes / (float)active_clients[i].mem_size_bytes) * 100.0f : 0;
                char ram_bar[16]; make_bar(vm_ram_pct, ram_bar, 10);
                printf(" %-6d | %-4d | %-16s | %s %5.1f%% | %s %4llu/%4llu MB | %-5d | %-10s\n", 
                       active_clients[i].client_id,
                       active_clients[i].core_id,
                       active_clients[i].payload_name,
                       vcpu_bar, active_clients[i].cpu_usage_pct,
                       ram_bar, (unsigned long long)(active_clients[i].used_mem_bytes / (1024*1024)),
                       (unsigned long long)(active_clients[i].mem_size_bytes / (1024*1024)),
                       active_clients[i].exits_handled,
                       active_clients[i].status);
            }
        }
        printf("=======================================================================================================================\n");
        pthread_mutex_unlock(&stats_mutex);
        usleep(100000); // 100ms refresh rate
    }
    return NULL;
}

void* vcpu_thread(void* arg) {
    vcpu_thread_arg_t *varg = (vcpu_thread_arg_t *)arg;
    client_req_t *req = varg->req;
    int client_id = req->client_id;
    int core_id = varg->core_id;
    int fd = req->fd;
    uint64_t phys_base = req->phys_base;
    uint64_t vm_mem_size_bytes = req->vm_mem_size_bytes;
    
    // Find a slot in the dashboard
    int slot = -1;
    pthread_mutex_lock(&stats_mutex);
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (!active_clients[i].active) {
            slot = i;
            active_clients[i].active = 1;
            active_clients[i].client_id = client_id;
            active_clients[i].core_id = core_id;
            strncpy(active_clients[i].payload_name, get_basename(req->payload_path), 63);
            active_clients[i].payload_name[63] = '\0';
            active_clients[i].phys_base = phys_base;
            active_clients[i].mem_size_bytes = vm_mem_size_bytes;
            active_clients[i].used_mem_bytes = 0;
            active_clients[i].exits_handled = 0;
            active_clients[i].busy_time_ns = 0;
            active_clients[i].idle_time_ns = 0;
            active_clients[i].cpu_usage_pct = 0.0f;
            strncpy(active_clients[i].status, "Initializing", 63);
            break;
        }
    }
    pthread_mutex_unlock(&stats_mutex);
    
    if (slot == -1) {
        dprintf(fd, "[VCPU-%d:%d] Server full.\n", client_id, core_id);
        goto cleanup;
    }

    update_status(slot, "Creating VCPU");
    hv_vcpu_t vcpu;
#if defined(__aarch64__)
    hv_vcpu_exit_t *vcpu_exit = NULL;
    hv_return_t ret = hv_vcpu_create(&vcpu, &vcpu_exit, NULL);
    varg->vcpu = vcpu;
#else
    hv_return_t ret = hv_vcpu_create(&vcpu, HV_VCPU_DEFAULT);
    varg->vcpu = vcpu;
#endif
    if (ret != HV_SUCCESS) {
        dprintf(fd, "[VCPU-%d:%d] Failed to create vCPU.\n", client_id, core_id);
        update_status(slot, "VCPU Create Failed");
        goto cleanup;
    }
    
#if defined(__aarch64__)
    hv_vcpu_set_reg(vcpu, HV_REG_PC, phys_base);
    
    // INJECT CORE ID INTO x0
    hv_vcpu_set_reg(vcpu, HV_REG_X0, core_id); 
    
    // Inject custom args into x1 and x2
    hv_vcpu_set_reg(vcpu, HV_REG_X1, req->arg0);
    hv_vcpu_set_reg(vcpu, HV_REG_X2, req->arg1);
    
    // INJECT DYNAMIC RAM SIZE INTO x3 FOR THE C KERNEL ALLOCATOR!
    hv_vcpu_set_reg(vcpu, HV_REG_X3, vm_mem_size_bytes);
    
    hv_vcpu_set_reg(vcpu, HV_REG_CPSR, 0x3c4);
    
    // Give each core its own 64KB stack descending from the top of dynamic memory!
    uint64_t stack_base = phys_base + vm_mem_size_bytes - (core_id * 65536);
    hv_return_t ret1 = hv_vcpu_set_sys_reg(vcpu, HV_SYS_REG_SP_EL0, stack_base);
    hv_return_t ret2 = hv_vcpu_set_sys_reg(vcpu, HV_SYS_REG_SP_EL1, stack_base);
    if (ret1 != HV_SUCCESS || ret2 != HV_SUCCESS) {
        dprintf(fd, "Failed to set SP! ret1=%x ret2=%x\n", ret1, ret2);
    }
    hv_vcpu_set_sys_reg(vcpu, HV_SYS_REG_VBAR_EL1, phys_base);
#endif

    update_status(slot, "Running");
    if (core_id == 0) {
        dprintf(fd, "[VM-%d] Booting up with %d VCPUs and %llu MB of RAM!\n", 
                client_id, req->num_cores, (unsigned long long)req->ram_mb);
    }
    
    struct timespec start_run, end_run, start_idle, end_idle;
    while (1) { 
        if (varg->timer_pending) {
            hv_vcpu_set_pending_interrupt(vcpu, HV_INTERRUPT_TYPE_IRQ, true);
        }
        
        clock_gettime(CLOCK_MONOTONIC, &start_run);
        ret = hv_vcpu_run(vcpu);
        clock_gettime(CLOCK_MONOTONIC, &end_run);
        
        uint64_t run_ns = (end_run.tv_sec - start_run.tv_sec) * 1000000000ULL + (end_run.tv_nsec - start_run.tv_nsec);
        pthread_mutex_lock(&stats_mutex);
        active_clients[slot].busy_time_ns += run_ns;
        pthread_mutex_unlock(&stats_mutex);
        
        if (ret != HV_SUCCESS) break;
#if defined(__aarch64__)
        if (vcpu_exit->reason == HV_EXIT_REASON_CANCELED) {
            continue; // Handled before next run
        } else if (vcpu_exit->reason == HV_EXIT_REASON_EXCEPTION) {
            uint64_t syndrome = vcpu_exit->exception.syndrome;
            uint32_t ec = (syndrome >> 26) & 0x3f;
            
            if (ec == 0x16) { // HVC Trap
                // ESR ISS[15:0] carries the HVC imm16 (observed: syndrome
                // 0x5a000005 for hvc #5). On HVC exits PC already points at
                // the instruction AFTER the hvc (ELR_EL2 semantics), so
                // resuming must not advance PC or a guest instruction is
                // skipped.
                uint32_t imm = syndrome & 0xffff;
                if (imm == 1) { 
                    dprintf(fd, "\n[VCPU-%d:%d] Core signaled graceful shutdown.\n", client_id, core_id);
                    break;
                } else if (imm == 2) {
                    dprintf(fd, "\n[VCPU-%d:%d] FATAL: Guest jumped to unhandled exception vector!\n", client_id, core_id);
                    break;
                } else if (imm == 3) {
                    varg->timer_pending = 0;
                    hv_vcpu_set_pending_interrupt(vcpu, HV_INTERRUPT_TYPE_IRQ, false);
                    continue; 
                } else if (imm == 4) {
                    if (!varg->timer_active &&
                        pthread_create(&varg->timer_tid, NULL, timer_irq_thread, (void *)varg) == 0) {
                        varg->timer_active = 1;
                    }
                    continue;
                } else if (imm == 99) { // Idle Task trap
                    clock_gettime(CLOCK_MONOTONIC, &start_idle);
                    usleep(1000);
                    clock_gettime(CLOCK_MONOTONIC, &end_idle);
                    
                    uint64_t idle_ns = (end_idle.tv_sec - start_idle.tv_sec) * 1000000000ULL + (end_idle.tv_nsec - start_idle.tv_nsec);
                    pthread_mutex_lock(&stats_mutex);
                    active_clients[slot].idle_time_ns += idle_ns;
                    pthread_mutex_unlock(&stats_mutex);
                    
                    continue;
                } else if (imm == 5) { // RAM REPORTING
                    uint64_t x0_val = 0;
                    hv_vcpu_get_reg(vcpu, HV_REG_X0, &x0_val);
                    pthread_mutex_lock(&stats_mutex);
                    active_clients[slot].used_mem_bytes = x0_val;
                    pthread_mutex_unlock(&stats_mutex);
                    continue;
                } else if (imm == 42) {
                    uint64_t x0_val = 0;
                    hv_vcpu_get_reg(vcpu, HV_REG_X0, &x0_val);
                    dprintf(fd, "[GUEST DEBUG] ESR=0x%llx\n", (unsigned long long)x0_val);
                    continue;
                }
                
                uint64_t x0_val = 0;
                hv_vcpu_get_reg(vcpu, HV_REG_X0, &x0_val);
                
                dprintf(fd, "[VCPU-%d:%d] HVC Trap: x0=%llu, syndrome=0x%llx, imm=0x%x\n", client_id, core_id, (unsigned long long)x0_val, (unsigned long long)syndrome, imm);
                
                pthread_mutex_lock(&stats_mutex);
                active_clients[slot].exits_handled++;
                snprintf(active_clients[slot].status, 63, "HVC Trap (x0: %llu)", (unsigned long long)x0_val);
                pthread_mutex_unlock(&stats_mutex);
                
                usleep(500000);
            } else if (ec == 0x01) { // WFI (Wait For Interrupt) Trap
                // Ignore, handled by HVC 99
            } else if (ec == 0x24) { // Data Abort (MMIO)
                uint64_t fault_addr = vcpu_exit->exception.physical_address;
                int is_write = (syndrome & (1 << 6));
                uint32_t srt = (syndrome >> 16) & 0x1f; 

                if (fault_addr == 0x100000000ULL) { // UART Data Register
                    if (is_write) { 
                        uint64_t val = 0;
                        hv_vcpu_get_reg(vcpu, HV_REG_X0 + srt, &val); 
                        char c = (char)val;
                        
                        dprintf(fd, "%c", c);
                        
                        pthread_mutex_lock(&stats_mutex);
                        active_clients[slot].exits_handled++;
                        snprintf(active_clients[slot].status, 63, "UART TX: '%c'", c == '\n' ? ' ' : c);
                        int current_exits = active_clients[slot].exits_handled;
                        pthread_mutex_unlock(&stats_mutex);
                        
                        if (c == '.' && current_exits % 30 == 0) {
                            dprintf(fd, "\r\n\r\n[Daemon] *** TIMER FIRED: ASSERTING HARDWARE IRQ PIN! ***\r\n");
                            hv_vcpu_set_pending_interrupt(vcpu, HV_INTERRUPT_TYPE_IRQ, true);
                        }
                    } else { // UART RX (Read)
                        pthread_mutex_lock(&stats_mutex);
                        active_clients[slot].exits_handled++;
                        pthread_mutex_unlock(&stats_mutex);
                        char c = 0;
                        recv(fd, &c, 1, MSG_DONTWAIT);
                        hv_vcpu_set_reg(vcpu, HV_REG_X0 + srt, c);
                    }
                    uint64_t pc = 0; hv_vcpu_get_reg(vcpu, HV_REG_PC, &pc); 
                    hv_vcpu_set_reg(vcpu, HV_REG_PC, pc + 4);
                } else {
                    uint64_t pc = 0; hv_vcpu_get_reg(vcpu, HV_REG_PC, &pc);
                    uint64_t v_fault_addr = vcpu_exit->exception.virtual_address;
                    dprintf(fd, "\r\n[VCPU-%d:%d] Unhandled Data Abort at PA: 0x%llx, VA: 0x%llx, PC: 0x%llx, Syndrome: 0x%llx\r\n", 
                            client_id, core_id, (unsigned long long)fault_addr, (unsigned long long)v_fault_addr, (unsigned long long)pc, (unsigned long long)vcpu_exit->exception.syndrome);
                    printf("[DAEMON EXIT] Unhandled Data Abort at PA: %llx, PC: %llx\n", fault_addr, pc);
                    
                    uint32_t instr1 = 0, instr2 = 0;
                    if (pc >= phys_base + 4 && pc < phys_base + vm_mem_size_bytes) {
                        instr1 = *(uint32_t*)((char*)varg->req->host_mem + (pc - 4 - phys_base));
                        instr2 = *(uint32_t*)((char*)varg->req->host_mem + (pc - phys_base));
                        dprintf(fd, "Instruction at PC-4: 0x%08x\r\n", instr1);
                        dprintf(fd, "Instruction at PC: 0x%08x\r\n", instr2);
                    }
                    
                    uint64_t regs[10];
                    for(int i=0; i<10; i++) {
                        hv_vcpu_get_reg(vcpu, HV_REG_X0 + i, &regs[i]);
                    }
                    uint64_t elr_el1 = 0; hv_vcpu_get_sys_reg(vcpu, HV_SYS_REG_ELR_EL1, &elr_el1);
                    uint64_t sp_el1 = 0; hv_vcpu_get_sys_reg(vcpu, HV_SYS_REG_SP_EL1, &sp_el1);
                    uint64_t sp_el0 = 0; hv_vcpu_get_sys_reg(vcpu, HV_SYS_REG_SP_EL0, &sp_el0);
                    dprintf(fd, "Registers:\r\nX0=%llx X1=%llx X2=%llx X3=%llx X4=%llx X5=%llx X6=%llx X7=%llx X8=%llx X9=%llx\r\nSP_EL1=%llx SP_EL0=%llx\r\n", 
                            regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7], regs[8], regs[9], sp_el1, sp_el0);

                    update_status(slot, "Data Abort");
                    break;
                }
            } else {
                printf("[DAEMON EXIT] Unhandled Exception EC=0x%x at PC=?\n", ec);
                update_status(slot, "Exception");
                break;
            }
        } else if (vcpu_exit->reason != HV_EXIT_REASON_CANCELED) {
            printf("[DAEMON EXIT] Unhandled Exit Reason=%x\n", vcpu_exit->reason);
            update_status(slot, "Error");
            break;
        }
#endif
    }
    
    update_status(slot, "Terminated");
    if (varg->timer_active) {
        varg->timer_stop = 1;
        pthread_join(varg->timer_tid, NULL);
    }
    hv_vcpu_destroy(vcpu);

cleanup:
    free(varg);
    int remaining = atomic_fetch_sub(&req->active_cores, 1) - 1;
    if (remaining == 0) {
        dprintf(fd, "\n[Daemon] All cores terminated. Tearing down VM and freeing %llu MB.\n", (unsigned long long)req->ram_mb);
        hv_vm_unmap(req->phys_base, req->vm_mem_size_bytes);
        free(req->host_mem);
        close(fd);
        free(req);
    }
    return NULL;
}

int main() {
#if defined(__aarch64__)
    if (hv_vm_create(NULL) != HV_SUCCESS) {
#else
    if (hv_vm_create(HV_VM_DEFAULT) != HV_SUCCESS) {
#endif
        printf("Failed to provision system VM capability.\n");
        return 1;
    }
    
    memset(active_clients, 0, sizeof(active_clients));
    
    pthread_t mon_tid;
    pthread_create(&mon_tid, NULL, monitor_thread, NULL);
    
    int server_fd, client_fd;
    struct sockaddr_un addr;
    
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    unlink(SOCKET_PATH);
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);
    
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    
    int client_counter = 1;
    
    while (1) {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        
        // We need non-blocking reads but blocking writes.
        // Easiest is to set a small read timeout instead of O_NONBLOCK.
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000; // 1ms timeout for reads
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        client_req_t *req = malloc(sizeof(client_req_t));
        // Calculate the exact footprint of the struct that arrives from the client over the wire.
        // 256 (path) + 8 (arg0) + 8 (arg1) + 8 (ram_mb) + 4 (num_cores) + 4 (client_id) + 4 (fd) = 292 bytes
        read(client_fd, req, 292); 
        
        req->client_id = client_counter++;
        req->fd = client_fd;
        
        if (req->num_cores <= 0 || req->num_cores > 8) req->num_cores = 1;
        if (req->ram_mb <= 0) req->ram_mb = 1;
        
        req->active_cores = req->num_cores;
        req->vm_mem_size_bytes = req->ram_mb * 1024 * 1024;
        
        // Provision Custom Memory Size!
        size_t page_size = sysconf(_SC_PAGESIZE);
        posix_memalign(&req->host_mem, page_size, req->vm_mem_size_bytes);
        memset(req->host_mem, 0, req->vm_mem_size_bytes);
        
        pthread_mutex_lock(&mem_mutex);
        req->phys_base = next_phys_base;
        next_phys_base += req->vm_mem_size_bytes;
        pthread_mutex_unlock(&mem_mutex);
        
        hv_vm_map(req->host_mem, req->phys_base, req->vm_mem_size_bytes, HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC);
        
        FILE *f = fopen(req->payload_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
            fread(req->host_mem, 1, fsize, f);
            fclose(f);
        } else {
            dprintf(client_fd, "Failed to open payload.\n");
            hv_vm_unmap(req->phys_base, req->vm_mem_size_bytes);
            free(req->host_mem);
            close(client_fd);
            free(req);
            continue;
        }
        
        // Spin up VCPUs
        for (int i = 0; i < req->num_cores; i++) {
            vcpu_thread_arg_t *varg = malloc(sizeof(vcpu_thread_arg_t));
            varg->req = req;
            varg->core_id = i;
            varg->rx_char = 0;
            varg->has_rx = 0;
            varg->timer_pending = 0;
            varg->timer_active = 0;
            varg->timer_stop = 0;
            pthread_t tid;
            pthread_create(&tid, NULL, vcpu_thread, varg);
            pthread_detach(tid);
        }
    }
    
    return 0;
}
