#ifndef UI_H
#define UI_H

#include <Windows.h>
#include "requests.h"
#include "config.h"

// Console colors
#define CONSOLE_COLOR_NORMAL    (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CONSOLE_COLOR_HEADER    (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_SELECTED  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_BLUE)
#define CONSOLE_COLOR_FRESH     (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_DAY_OLD   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_ERROR     (FOREGROUND_RED | FOREGROUND_INTENSITY)

// UI modes
typedef enum {
    MODE_TABLE,
    MODE_RELEASE_PAGE,
    MODE_TAG_DROPDOWN
} UIMode;

typedef struct {
    HANDLE hConsole;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int console_width;
    int console_height;
    int selected_row;
    int table_start_row;
    int visible_rows;
    int total_rows;
    UIMode current_mode;
    ReleaseCollection* releases;
    Config* config;
} UIState;

// Function declarations
void init_ui(void);
void cleanup_ui(void);
void setup_colors(void);
void setup_windows_console(void);

UIState* create_ui_state(Config* config, ReleaseCollection* releases);
void free_ui_state(UIState* state);

void draw_header(UIState* state, const char* title);
void draw_footer(UIState* state, UIMode mode);
void draw_table(UIState* state);
void draw_table_row(UIState* state, int row, Release* release, bool selected);
void update_display(UIState* state);

void handle_table_input(UIState* state, int ch);
void handle_input(UIState* state);

void center_text(UIState* state, int row, const char* text);
void set_loading_message(UIState* state, int row, const char* owner, const char* repo);

// Console utility functions
void clear_console(UIState* state);
void set_console_cursor_position(UIState* state, int x, int y);
void set_console_color(UIState* state, WORD color);
void print_at(UIState* state, int x, int y, const char* text);
void print_colored_at(UIState* state, int x, int y, const char* text, WORD color);
int getch(void);

#endif // UI_H