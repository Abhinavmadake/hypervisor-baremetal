import sys
import struct

if len(sys.argv) < 3:
    sys.exit(1)

in_file = sys.argv[1]
out_file = sys.argv[2]

with open(in_file, 'rb') as f:
    macho = f.read()

magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved = struct.unpack('<I II II II I', macho[:32])

min_vmaddr = 0xFFFFFFFFFFFFFFFF
segments = []
text_vmaddr = None

offset = 32
for i in range(ncmds):
    cmd, cmdsize = struct.unpack('<II', macho[offset:offset+8])
    if cmd == 0x19: # LC_SEGMENT_64
        segname = macho[offset+8:offset+24].rstrip(b'\0')
        vmaddr, vmsize, fileoff, filesize = struct.unpack('<QQQQ', macho[offset+24:offset+56])
        if segname in [b'__TEXT', b'__DATA', b''] and vmsize > 0:
            if vmaddr < min_vmaddr:
                min_vmaddr = vmaddr
            segments.append((vmaddr, vmsize, fileoff, filesize))
            
            # Find __text section inside __TEXT segment or object file segment
            if segname in [b'__TEXT', b'']:
                nsects = struct.unpack('<I', macho[offset+64:offset+68])[0]
                sect_off = offset + 72
                for j in range(nsects):
                    sectname = macho[sect_off:sect_off+16].rstrip(b'\0')
                    addr = struct.unpack('<Q', macho[sect_off+32:sect_off+40])[0]
                    if sectname == b'__text':
                        text_vmaddr = addr
                    sect_off += 80
                    
    offset += cmdsize

raw_bin = bytearray()
for vmaddr, vmsize, fileoff, filesize in segments:
    seg_offset = vmaddr - min_vmaddr
    if len(raw_bin) < seg_offset:
        raw_bin.extend(b'\0' * (seg_offset - len(raw_bin)))
    data = macho[fileoff:fileoff+filesize]
    raw_bin[seg_offset:seg_offset+len(data)] = data
    if len(raw_bin) < seg_offset + vmsize:
        raw_bin.extend(b'\0' * (seg_offset + vmsize - len(raw_bin)))

if text_vmaddr is not None:
    # Overwrite the first 4 bytes with a branch instruction to __text
    entry_offset = text_vmaddr - min_vmaddr
    if entry_offset > 0:
        imm26 = entry_offset // 4
        b_instr = 0x14000000 | (imm26 & 0x03FFFFFF)
        struct.pack_into('<I', raw_bin, 0, b_instr)

with open(out_file, 'wb') as f:
    f.write(raw_bin)

print(f"Extracted {len(raw_bin)} bytes")
