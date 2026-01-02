import struct
import os

# Configuration - MUST match build.sh and interrupt_handler.c
DISK_IMG = "cadence_disk.img" 
ENTRIES_SECTOR = 2
ENTRY_SIZE = 20  
APP_START_SECTOR = 50 # Matching your build.sh 'seek=50'

def add_file_to_disk(filename, bin_path, sector):
    if not os.path.exists(bin_path):
        print(f"Error: {bin_path} not found!")
        return

    # 1. Read the binary code
    with open(bin_path, "rb") as f:
        binary_data = f.read()

    # 2. Open disk in r+b (read/write binary)
    with open(DISK_IMG, "r+b") as f:
        # Write binary to the app sector
        f.seek(sector * 512)
        f.write(binary_data)
        
        # 3. Update the FileEntry table at Sector 2 
        f.seek(ENTRIES_SECTOR * 512)
        # Read all 25 possible entries
        entries_data = bytearray(f.read(25 * ENTRY_SIZE))
        
        found_slot = False
        for i in range(25):
            offset = i * ENTRY_SIZE
            # Unpack the 'active' field (last 4 bytes of the 20-byte struct)
            active = struct.unpack_from("<I", entries_data, offset + 16)[0]
            
            # Check if slot is empty OR if this file already exists (to overwrite)
            current_name = struct.unpack_from("<12s", entries_data, offset)[0].split(b'\0')[0].decode('ascii')
            
            if active == 0 or current_name == filename: 
                # Pack: name[12s], sector[I], active[I] 
                # Ensure name is exactly 12 bytes, null-padded
                name_bytes = filename.encode('ascii')[:11].ljust(12, b'\0')
                struct.pack_into("<12sII", entries_data, offset, name_bytes, sector, 1)
                found_slot = True
                break
        
        if found_slot:
            f.seek(ENTRIES_SECTOR * 512)
            f.write(entries_data)
            print(f"Successfully linked {filename} to sector {sector}")
        else:
            print("Error: No empty slots in file table!")

# Run the injection
add_file_to_disk("snake.link", "snake.link", APP_START_SECTOR)