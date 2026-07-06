#!/bin/bash
set -e

echo ""
echo "  ╔══════════════════════════════════════════════╗"
echo "  ║   Bare-Metal macOS Hypervisor (Apple Silicon) ║"
echo "  ╚══════════════════════════════════════════════╝"
echo ""

# Build
echo "[1/3] Building..."
make -s
echo "      Done."

# Kill any stale daemon
echo "[2/3] Cleaning up old daemon..."
pkill -f hv_daemon 2>/dev/null || true
sleep 0.5

# Launch daemon
PROJECT_DIR=$(pwd)
echo "[3/3] Launching Hypervisor Daemon in new terminal..."
osascript -e 'tell app "Terminal" to do script "cd '"$PROJECT_DIR"' && ./hv_daemon"'
sleep 2
echo "      Daemon is ready."

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Select a mode:"
echo ""
echo "  1) Interactive  — Single VM, you control the shell"
echo "  2) Multi-VM     — Spawn N VMs with auto-load"
echo ""
read -p "  Mode [1/2]: " mode
mode=${mode:-1}

if [ "$mode" = "1" ]; then
    echo ""
    read -p "  RAM size in MB (default: 128): " ram_size
    ram_size=${ram_size:-128}
    echo ""
    echo "  Booting guest OS with ${ram_size} MB RAM..."
    osascript -e 'tell app "Terminal" to do script "cd '"$PROJECT_DIR"' && ./hv_client client_ckernel.bin 0 0 1 '"$ram_size"'"'

elif [ "$mode" = "2" ]; then
    echo ""
    read -p "  Number of VMs to spawn (default: 2): " num_vms
    num_vms=${num_vms:-2}
    read -p "  RAM per VM in MB (default: 64): " ram_size
    ram_size=${ram_size:-64}
    read -p "  CPU load per VM in % (default: 50): " cpu_load
    cpu_load=${cpu_load:-50}
    read -p "  Memory load per VM in MB (0 = none, default: 0): " mem_load
    mem_load=${mem_load:-0}

    echo ""
    echo "  Spawning ${num_vms} VMs — ${ram_size} MB RAM each — load ${cpu_load}% CPU / ${mem_load} MB mem"
    echo ""

    for i in $(seq 1 "$num_vms"); do
        echo "  -> VM $i launching..."
        # Build the auto-load command to pipe into the client
        # sleep gives the kernel time to boot, then we send the load command
        # 'cat' keeps stdin open so the client stays alive for UART output
        osascript -e 'tell app "Terminal" to do script "cd '"$PROJECT_DIR"' && (sleep 2; echo '"'load $cpu_load $mem_load'"'; cat) | ./hv_client client_ckernel.bin 0 0 1 '"$ram_size"'"'
        sleep 1
    done

    echo ""
    echo "  All ${num_vms} VMs launched. Check the daemon dashboard."
else
    echo "  Invalid mode."
    exit 1
fi

echo ""
