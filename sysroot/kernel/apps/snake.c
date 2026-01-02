#include <stdint.h>

/* --- PROTOTYPES --- */
void main_game_logic(void);
static inline uint8_t inb(uint16_t port);
void draw_char(int x, int y, char c, uint8_t color);
void clear_game_screen(void);

/* --- ENTRY POINT --- */
/**
 * This MUST be the first function in the binary.
 * The section attribute ensures the linker puts this at the absolute top.
 */
/* Ensure this attribute matches the *(.text.prologue) in your apps.ld */
void __attribute__((section(".text.prologue"))) _start(void) {
    main_game_logic();
    return;
}

/* --- LOW LEVEL I/O --- */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* --- GAME CONSTANTS --- */
#define VIDEO_MEM 0xB8000
#define COLS 80
#define ROWS 25
#define SNAKE_MAX_LEN 100

#define KEY_UP    0x48
#define KEY_DOWN  0x50
#define KEY_LEFT  0x4B
#define KEY_RIGHT 0x4D
#define KEY_ESC   0x01

struct Point {
    int x, y;
};

/* --- STATE (Static to stay off the stack) --- */
static struct Point snake[SNAKE_MAX_LEN];
static int snake_len = 3;
static int dx = 1, dy = 0;
static struct Point food = {15, 10};
static uint32_t seed = 0x12345;

/* --- HELPERS --- */
void draw_char(int x, int y, char c, uint8_t color) {
    uint16_t* terminal_buffer = (uint16_t*)VIDEO_MEM;
    terminal_buffer[y * COLS + x] = (uint16_t)c | (uint16_t)color << 8;
}

void clear_game_screen(void) {
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            draw_char(x, y, ' ', 0x07);
        }
    }
}

/* --- GAME LOGIC --- */
void main_game_logic(void) {
    // Initialize Snake State 
    snake_len = 3;
    dx = 1; dy = 0;
    for(int i = 0; i < snake_len; i++) {
        snake[i].x = 10 - i;
        snake[i].y = 10;
    }

    clear_game_screen();

    while(1) {
        // 1. Input: Direct Port I/O 
        uint8_t scancode = inb(0x60);
        if (scancode == KEY_UP && dy == 0)          { dx = 0;  dy = -1; }
        else if (scancode == KEY_DOWN && dy == 0)  { dx = 0;  dy = 1;  }
        else if (scancode == KEY_LEFT && dx == 0)  { dx = -1; dy = 0;  }
        else if (scancode == KEY_RIGHT && dx == 0) { dx = 1;  dy = 0;  }
        else if (scancode == KEY_ESC) return; 

        // 2. Erase Tail 
        draw_char(snake[snake_len-1].x, snake[snake_len-1].y, ' ', 0x07);

        // 3. Move Body 
        for(int i = snake_len - 1; i > 0; i--) {
            snake[i] = snake[i-1];
        }

        // 4. Move Head 
        snake[0].x += dx;
        snake[0].y += dy;

        // 5. Wrap around edges 
        if (snake[0].x >= COLS) snake[0].x = 0;
        else if (snake[0].x < 0) snake[0].x = COLS - 1;
        if (snake[0].y >= ROWS) snake[0].y = 0;
        else if (snake[0].y < 0) snake[0].y = ROWS - 1;

        // 6. Food Collision and Growth 
        if (snake[0].x == food.x && snake[0].y == food.y) {
            if (snake_len < SNAKE_MAX_LEN) snake_len++;
            seed = seed * 1103515245 + 12345;
            food.x = (seed / 65536) % (COLS - 2) + 1;
            food.y = (seed / 65536) % (ROWS - 2) + 1;
        }

        // 7. Draw Frame 
        draw_char(food.x, food.y, '@', 0x0E); // Food (Yellow)
        draw_char(snake[0].x, snake[0].y, '#', 0x0A); // Head (Green)

        // 8. Game Speed 
        for(volatile int d = 0; d < 55000000; d++); 
    }
}