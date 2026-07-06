# Bare-Metal macOS Hypervisor (Apple Silicon)

A fully functional, hardware-accelerated Type-2 Hypervisor built from scratch on macOS using the native `Hypervisor.framework`. It boots a bare-metal ARM64 OS kernel with an interactive shell, preemptive multitasking, and dynamic memory allocation — all running directly on Apple Silicon hardware without emulation.

---

## Features

- **True Bare-Metal Execution**: ARM64 guest code runs natively on the Apple Silicon CPU via `Hypervisor.framework`.
- **Virtual Memory Provisioning**: Dynamically maps isolated physical memory blocks (configurable size) for guest VMs.
- **Memory-Mapped I/O (MMIO)**: Intercepts Data Aborts to simulate a Virtual UART Serial Port for bidirectional console I/O.
- **Symmetric Multiprocessing (SMP)**: Spawns multi-core VMs sharing the same physical memory on separate host threads with dynamically offset stack pointers.
- **Hardware Interrupt Injection**: Implements an ARM64 Exception Vector Table with OS-level hardware timer ticks via virtual IRQ injection for preemptive scheduling.
- **Preemptive Multitasking**: Round-robin scheduler with sleep/wake support, background tasks, and cooperative + preemptive context switching.
- **Dynamic Memory Allocator**: Simple bump allocator with heap tracking and `malloc` support inside the guest OS.
- **Live Dashboard**: ANSI-based terminal dashboard showing active VCPUs, CPU usage, RAM consumption, and trap counts in real time.

---

## Project Structure

| File | Description |
|------|-------------|
| `hv_daemon.c` | Hypervisor host — provisions VMs, handles traps (HVC, MMIO, IRQ), runs the live dashboard |
| `hv_client.c` | CLI client — sends payloads to the daemon and streams UART I/O to/from the terminal |
| `boot.s` | ARM64 bootstrap assembly — sets up stack, exception vectors, context switch routines |
| `kernel.c` | Bare-metal C kernel — interactive shell, task scheduler, memory allocator, MMU setup |
| `extract_bin.py` | Strips Mach-O headers to produce a flat binary blob for guest execution |
| `linker.ld` | Linker script targeting the guest physical address space (`0x80000000`) |
| `entitlements.plist` | macOS entitlements required for `Hypervisor.framework` access |
| `Makefile` | Builds everything: daemon, client, and the guest kernel binary |
| `run.sh` | One-command build & launch script |

---

## Quick Start

```bash
cd hv_project
./run.sh
```

This will build the project, launch the hypervisor daemon in a new terminal window, and prompt you for a RAM size to boot the guest OS.

---

## Manual Usage

**1. Start the Hypervisor Daemon** (in a dedicated terminal):
```bash
./hv_daemon
```

**2. Launch a VM** (in another terminal):
```bash
./hv_client client_ckernel.bin [arg1] [arg2] [num_cores] [ram_mb]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `arg1` | 0 | Value injected into guest register `x0` |
| `arg2` | 0 | Value injected into guest register `x1` |
| `num_cores` | 1 | Number of VCPUs (max 8) |
| `ram_mb` | 1 | RAM allocated to the VM in MB |

**Example:**
```bash
./hv_client client_ckernel.bin 0 0 1 128
```
Boots the bare-metal OS with 1 core and 128 MB of RAM.

---

## Guest OS Shell Commands

Once the VM boots, you get an interactive shell (`root@hv-guest:~#`):

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `meminfo` | Display physical memory layout (base, heap, limit) |
| `echo <text>` | Print text to the console |
| `malloc` | Allocate 1024 bytes from the heap |
| `run A` | Start background Task A (prints periodically) |
| `run B` | Start background Task B (prints periodically) |
| `load <cpu%> [mem_mb]` | Start a load task consuming CPU% and optionally allocating memory |
| `tasks` | List all running background tasks |
| `kill <id>` | Terminate a background task by ID |
| `clear` | Clear the screen |
| `halt` | Gracefully shut down the VM |

---

## How It Works

### Hypervisor ↔ Guest Communication

The guest communicates with the hypervisor via `HVC` (Hypervisor Call) traps:

| HVC # | Purpose |
|-------|---------|
| `hvc #1` | Graceful shutdown |
| `hvc #2` | Fatal exception report |
| `hvc #3` | Acknowledge timer IRQ |
| `hvc #4` | Start hardware timer thread |
| `hvc #5` | Report RAM usage (x0 = bytes used) |
| `hvc #42` | Debug: print ESR value |
| `hvc #99` | Idle task yield (sleep host-side) |

### UART (Virtual Serial Port)

Guest reads/writes to physical address `0x100000000` are intercepted as Data Aborts and routed through the daemon as a virtual UART — providing bidirectional console I/O between the guest shell and the host terminal.

### Preemptive Scheduling

The daemon spawns a `timer_irq_thread` that injects hardware IRQs into the guest at regular intervals. The guest's exception vector table handles these IRQs by saving/restoring full register context and calling the round-robin `schedule()` function to switch between tasks.

---

## Build Requirements

- macOS on Apple Silicon (M1/M2/M3/M4)
- Xcode Command Line Tools (`clang`, `ld`)
- Python 3 (for `extract_bin.py`)
