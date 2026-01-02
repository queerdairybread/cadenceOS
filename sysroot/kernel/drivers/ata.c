// Read a sector (512 bytes) from the Master Drive on the Primary Bus
void ata_read_sector(uint32_t lba, uint16_t* buffer) {
    outb(0x1F6, (uint8_t)((lba >> 24) & 0x0F) | 0xE0); // Select drive
    outb(0x1F2, 1);                                   // Read 1 sector
    outb(0x1F3, (uint8_t)lba);                        // LBA Low
    outb(0x1F4, (uint8_t)(lba >> 8));                 // LBA Mid
    outb(0x1F5, (uint8_t)(lba >> 16));                // LBA High
    outb(0x1F7, 0x20);                                // Command: Read with retry

    // Wait for the drive to be ready
    while (!(inb(0x1F7) & 0x08));

    // Transfer the data!
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(0x1F0); // Read 2 bytes at a time
    }
}