#include <stdint.h>

/* --- LOW LEVEL I/O --- */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void cli() { asm volatile("cli"); }
static inline void sti() { asm volatile("sti"); }

/* --- FILESYSTEM STRUCTURE --- */
// Added __attribute__((packed)) to ensure disk alignment
typedef struct {
    char name[12];
    uint32_t sector;
    uint32_t active;
} __attribute__((packed)) FileEntry; 

/* --- SYSTEM & EDITOR STATE --- */
int in_editor = 0;
uint32_t editing_sector = 0;
char file_buffer[512];
// GLOBAL BUFFER FIX: Moved entries array out of functions to prevent stack crashes
FileEntry entries_buffer[25]; 
int file_index = 0;

int cursor_x = 0;
int cursor_y = 0; 
char cmd_buffer[81];
int cmd_index = 0;

// NEW: Function pointer for jumping to loaded .link applications
typedef void (*app_entry_t)(void);

/* --- SCREEN CONTROL --- */
void update_cursor(int x, int y) {
    uint16_t pos = y * 80 + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void shell_print(const char* str, uint8_t color) {
    uint16_t* terminal_buffer = (uint16_t*)0xB8000;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            terminal_buffer[cursor_y * 80 + cursor_x] = (uint16_t)str[i] | (uint16_t)color << 8;
            cursor_x++;
        }
        if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
    }
}

void clear_screen() {
    uint16_t* terminal_buffer = (uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) terminal_buffer[i] = (uint16_t)' ' | (uint16_t)0x0F00;
    cursor_x = 0; cursor_y = 0;
}

void scroll() {
    uint16_t* terminal_buffer = (uint16_t*)0xB8000;
    for (int i = 0; i < 24 * 80; i++) terminal_buffer[i] = terminal_buffer[i + 80];
    for (int i = 24 * 80; i < 25 * 80; i++) terminal_buffer[i] = (uint16_t)' ' | (uint16_t)0x0F00;
}

/* --- BOOT SPLASH --- */
void draw_boot_splash() {
    clear_screen();
    shell_print("  ____          _                             ___  ____  \n", 0x0B); 
    shell_print(" / ___|__ _  __| | ___ _ __   ___ ___        / _ \\/ ___| \n", 0x0B);
    shell_print("| |   / _` |/ _` |/ _ \\ '_ \\ / __/ _ \\      | | | \\___ \\ \n", 0x03); 
    shell_print("| |__| (_| | (_| |  __/ | | | (_|  __/      | |_| |___) |\n", 0x03);
    shell_print(" \\____\\__,_|\\__,_|\\___|_| |_|\\___\\___|       \\___/|____/ \n", 0x09); 
    shell_print(" ----------------------------------------------------------- \n", 0x07);
    cursor_y = 7; cursor_x = 0;
    update_cursor(cursor_x, cursor_y);
}

/* --- DISK I/O --- */
void ata_wait_ready() {
    while (inb(0x1F7) & 0x80); 
    uint8_t status = inb(0x1F7);
    while (!(status & 0x08) && !(status & 0x01)) status = inb(0x1F7);
}

void read_sector(uint32_t sector, uint16_t* buf) {
    cli();
    outb(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)sector);
    outb(0x1F4, (uint8_t)(sector >> 8));
    outb(0x1F5, (uint8_t)(sector >> 16));
    outb(0x1F7, 0x20); 
    ata_wait_ready();
    for (int i = 0; i < 256; i++) buf[i] = inw(0x1F0);
    inb(0x1F7);
    sti();
}

void write_sector(uint32_t sector, uint16_t* buf) {
    cli();
    outb(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)sector);
    outb(0x1F4, (uint8_t)(sector >> 8));
    outb(0x1F5, (uint8_t)(sector >> 16));
    outb(0x1F7, 0x30); 
    ata_wait_ready();
    for (int i = 0; i < 256; i++) outw(0x1F0, buf[i]);
    outb(0x1F7, 0xE7); 
    inb(0x1F7);
    sti();
}

/* --- UTILS --- */
int kstrcmp(char* s1, char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return 0;
        i++;
    }
    return (s1[i] == s2[i]);
}

// NEW: Helper to find command length for extension checking
int kstrlen(char* s) {
    int len = 0;
    while(s[len] != '\0') len++;
    return len;
}

/* --- APPLICATION LOADER --- */
void launch_app(char* filename) {
    read_sector(2, (uint16_t*)entries_buffer);
    for(int i = 0; i < 25; i++) {
        if(entries_buffer[i].active == 1 && kstrcmp(filename, entries_buffer[i].name)) {
            uint32_t app_sector = entries_buffer[i].sector;
            uint16_t* load_addr = (uint16_t*)0x20000;

            // CHANGE: Read 8 sectors (4KB) to ensure the whole game is loaded
            for(int s = 0; s < 12; s++) {
                read_sector(app_sector + s, (uint16_t*)((uint32_t)load_addr + (s * 512)));
            }

            shell_print("Launching ", 0x0A);
            shell_print(filename, 0x0F);
            shell_print("...\n", 0x0A);

            asm volatile (
                "pusha\n\t"      // Save all kernel registers so the app doesn't break them
                "call *%0\n\t"   // Jump to the app at 0x20000
                "popa"           // Restore kernel registers when the app is done
                : : "r"(0x20000) : "memory"
            ); 

            shell_print("\nExited.\n", 0x0E);
            return;
        }
    }
    shell_print("Error: App not found.\n", 0x0C);
}

/* --- APPS --- */
void open_sing(char* filename) {
    read_sector(2, (uint16_t*)entries_buffer);
    for(int i = 0; i < 25; i++) {
        if(entries_buffer[i].active == 1 && kstrcmp(filename, entries_buffer[i].name)) {
            editing_sector = entries_buffer[i].sector;
            in_editor = 1; 
            for(int j=0; j<512; j++) file_buffer[j] = 0;
            read_sector(editing_sector, (uint16_t*)file_buffer);
            file_index = 0;
            while(file_buffer[file_index] != '\0' && file_index < 511) file_index++;
            clear_screen();
            shell_print("SING STUDIO - Editing: ", 0x0E); shell_print(filename, 0x0F);
            shell_print("\n------------------------------------------\n", 0x07);
            shell_print(file_buffer, 0x07);
            update_cursor(cursor_x, cursor_y);
            return;
        }
    }
    shell_print("Error: File not found.", 0x0C);
}

/* --- COMMAND PARSER --- */
void process_command() {
    cmd_buffer[cmd_index] = '\0';
    if (cmd_index == 0) {
        shell_print("\n", 0x07);
        return;
    }
    shell_print("\n", 0x07);

    // NEW: Check if the command entered is a .link file to be executed
    int len = kstrlen(cmd_buffer);
    if (len > 5 && kstrcmp(&cmd_buffer[len-5], ".link")) {
        launch_app(cmd_buffer);
    }
    else if (kstrcmp(cmd_buffer, "help")) {
        shell_print("LS, TOUCH <f>, SING <f>, CAT <f>, RM <f>, FORMAT, CLEAR, REBOOT\n", 0x0B);
    }
    else if (kstrcmp(cmd_buffer, "ls")) {
        read_sector(2, (uint16_t*)entries_buffer);
        shell_print("Files on Disk:\n", 0x0B);
        for(int i=0; i<25; i++) {
            if(entries_buffer[i].active == 1) {
                shell_print("- ", 0x07); 
                shell_print(entries_buffer[i].name, 0x0F);
                shell_print("\n", 0x07);
            }
        }
    }
    else if (cmd_buffer[0] == 's' && cmd_buffer[1] == 'i' && cmd_buffer[2] == 'n' && cmd_buffer[3] == 'g') {
        open_sing(&cmd_buffer[5]);
    }
    else if (cmd_buffer[0] == 'c' && cmd_buffer[1] == 'a' && cmd_buffer[2] == 't') {
        char* target = &cmd_buffer[4];
        read_sector(2, (uint16_t*)entries_buffer);
        int found = 0;
        for(int i=0; i<25; i++) {
            if(entries_buffer[i].active == 1 && kstrcmp(target, entries_buffer[i].name)) {
                read_sector(entries_buffer[i].sector, (uint16_t*)file_buffer);
                shell_print(file_buffer, 0x0F);
                shell_print("\n", 0x07);
                found = 1; break;
            }
        }
        if(!found) shell_print("File not found.\n", 0x0C);
    }
    else if (cmd_buffer[0] == 'r' && cmd_buffer[1] == 'm') {
        char* target = &cmd_buffer[3];
        read_sector(2, (uint16_t*)entries_buffer);
        for(int i=0; i<25; i++) {
            if(entries_buffer[i].active == 1 && kstrcmp(target, entries_buffer[i].name)) {
                entries_buffer[i].active = 0; write_sector(2, (uint16_t*)entries_buffer);
                shell_print("Removed.\n", 0x0A); break;
            }
        }
    }
    else if (kstrcmp(cmd_buffer, "format")) {
        for(int i=0; i<25; i++) entries_buffer[i].active = 0;
        write_sector(2, (uint16_t*)entries_buffer);
        shell_print("Disk Formatted.\n", 0x0A);
    }
    else if (cmd_buffer[0] == 't' && cmd_buffer[1] == 'o' && cmd_buffer[2] == 'u' && cmd_buffer[3] == 'c' && cmd_buffer[4] == 'h') {
        read_sector(2, (uint16_t*)entries_buffer);
        char* name = &cmd_buffer[6];
        for(int i=0; i<25; i++) if(entries_buffer[i].active != 1) {
            entries_buffer[i].active = 1; entries_buffer[i].sector = i + 10;
            int j; for(j=0; j<11 && name[j] != '\0'; j++) entries_buffer[i].name[j] = name[j];
            entries_buffer[i].name[j] = '\0';
            write_sector(2, (uint16_t*)entries_buffer);
            shell_print("Touched file.\n", 0x0A); break;
        }
    }
    else if (kstrcmp(cmd_buffer, "clear")) clear_screen();
    else if (kstrcmp(cmd_buffer, "reboot")) outb(0x64, 0xFE);
    else shell_print("Unknown Command.\n", 0x0C);

    if(!in_editor) {
        cmd_index = 0;
        for(int i = 0; i < 81; i++) cmd_buffer[i] = 0;
    }
}

/* --- KEYBOARD --- */
unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

void interrupt_handler(void) {
    uint8_t scancode = inb(0x60);
    outb(0x20, 0x20); outb(0xA0, 0x20);

    if (!(scancode & 0x80)) {
        if (scancode == 0x01) { // ESC
            if(in_editor) {
                file_buffer[file_index] = '\0';
                write_sector(editing_sector, (uint16_t*)file_buffer);
                in_editor = 0; draw_boot_splash();
                shell_print("Changes saved.\n> ", 0x0A);
                cmd_index = 0;
            }
            return;
        }

        char c = keyboard_map[scancode];
        if (in_editor) {
            if (c == '\b') {
                if (file_index > 0) {
                    file_index--;
                    file_buffer[file_index] = '\0';
                    if (cursor_x > 0) cursor_x--;
                    else if (cursor_y > 3) { cursor_y--; cursor_x = 79; }
                    ((uint16_t*)0xB8000)[cursor_y * 80 + cursor_x] = (uint16_t)' ' | 0x0F00;
                }
            }
            else if (c == '\n') { 
                cursor_y++; cursor_x = 0; 
                if(file_index < 511) file_buffer[file_index++] = '\n';
            }
            else if (c != 0 && file_index < 510) {
                file_buffer[file_index++] = c;
                ((uint16_t*)0xB8000)[cursor_y * 80 + cursor_x] = (uint16_t)c | 0x0F00;
                cursor_x++;
            }
        } else {
            if (c == '\n') { 
                process_command(); 
                if(!in_editor) shell_print("> ", 0x0E);
            }
            else if (c == '\b' && cmd_index > 0) {
                cmd_index--; cursor_x--;
                ((uint16_t*)0xB8000)[cursor_y * 80 + cursor_x] = (uint16_t)' ' | 0x0F00;
            } else if (c != 0 && cmd_index < 70) {
                cmd_buffer[cmd_index++] = c;
                ((uint16_t*)0xB8000)[cursor_y * 80 + cursor_x] = (uint16_t)c | 0x0F00;
                cursor_x++;
            }
        }
        if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
        if (cursor_y >= 25) { scroll(); cursor_y = 24; }
        update_cursor(cursor_x, cursor_y);
    }
}