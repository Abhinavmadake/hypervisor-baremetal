CC = clang
CFLAGS = -Wall -framework Hypervisor
ENTITLEMENTS = entitlements.plist

CLIENTS = client_ckernel.bin

all: hv_daemon hv_client $(CLIENTS) sign

hv_daemon: hv_daemon.c
	$(CC) $(CFLAGS) -o hv_daemon hv_daemon.c

hv_client: hv_client.c
	$(CC) -Wall -o hv_client hv_client.c

client_ckernel.bin: boot.s kernel.c
	clang -arch arm64 -c boot.s -o boot.o
	clang -arch arm64 -ffreestanding -fno-stack-protector -mno-implicit-float -c kernel.c -o kernel.o
	ld -static -e _start boot.o kernel.o -o kernel.macho
	python3 extract_bin.py kernel.macho client_ckernel.bin

sign: hv_daemon $(ENTITLEMENTS)
	codesign -s - --entitlements $(ENTITLEMENTS) --force hv_daemon

clean:
	rm -f hv_daemon hv_client *.o *.bin kernel.macho
