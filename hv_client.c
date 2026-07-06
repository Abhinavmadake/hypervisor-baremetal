#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <termios.h>
#include <signal.h>

typedef struct {
    char payload_path[256];
    uint64_t arg0;
    uint64_t arg1;
    uint64_t ram_mb;
    int num_cores;
    int client_id;
    int fd;
} client_req_t;

int sock_fd;
struct termios oldt;

void restore_term(int sig) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n[Client] Terminated.\n");
    exit(0);
}

// Background thread to read physical keystrokes and pipe them into the VM!
void* read_stdin(void* arg) {
    char c;
    while (read(STDIN_FILENO, &c, 1) > 0) {
        write(sock_fd, &c, 1);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <payload.bin> [arg1] [arg2] [num_cores] [ram_mb]\n", argv[0]);
        return 1;
    }
    
    client_req_t req;
    memset(&req, 0, sizeof(req));
    
    char *abs_path = realpath(argv[1], NULL);
    if (!abs_path) {
        printf("Invalid payload path: %s\n", argv[1]);
        return 1;
    }
    strncpy(req.payload_path, abs_path, sizeof(req.payload_path)-1);
    free(abs_path);
    
    if (argc > 2) req.arg0 = strtoull(argv[2], NULL, 0);
    if (argc > 3) req.arg1 = strtoull(argv[3], NULL, 0);
    if (argc > 4) req.num_cores = atoi(argv[4]);
    else req.num_cores = 1;
    
    if (argc > 5) req.ram_mb = strtoull(argv[5], NULL, 0);
    else req.ram_mb = 1; 
    
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/hv_daemon.sock", sizeof(addr.sun_path)-1);
    
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Failed to connect to Hypervisor Daemon. Is it running?\n");
        return 1;
    }
    
    // Transmit request to daemon. Send exactly the 292 wire bytes the daemon
    // reads (256 path + 8+8+8 args + 4+4+4 ints); sizeof(req) is 296 due to
    // tail padding, and the 4 extra bytes would leak into the guest UART RX.
    write(sock_fd, &req, 292);
    
    // --- ENABLE RAW TERMINAL MODE FOR TWO-WAY OS COMMUNICATION ---
    tcgetattr(STDIN_FILENO, &oldt);
    struct termios newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Disable buffering and echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    signal(SIGINT, restore_term);
    
    pthread_t tid;
    pthread_create(&tid, NULL, read_stdin, NULL);
    
    // Read response stream from hypervisor
    char buf[1024];
    int n;
    while ((n = read(sock_fd, buf, sizeof(buf))) > 0) {
        int written = 0;
        while (written < n) {
            int ret = write(STDOUT_FILENO, buf + written, n - written);
            if (ret <= 0) break;
            written += ret;
        }
    }
    
    restore_term(0);
    return 0;
}
