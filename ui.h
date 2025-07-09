#ifndef UI_H
#define UI_H

#include <curses.h>
#include "requests.h"
#include "config.h"

// Color pairs
#define COLOR_PAIR_NORMAL    1
#define COLOR_PAIR_HEADER    2
#define COLOR_PAIR_SELECTED  3
#define COLOR_PAIR_FRESH     4
#define COLOR_PAIR_DAY_OLD   5
#define COLOR_PAIR_ERROR     6

// UI modes
typedef enum {
    MODE_TABLE,
    MODE_RELEASE_PAGE,
    MODE_TAG_DROPDOWN
} UIMode;

typedef struct {
    WINDOW* main_win;
    WINDOW* header_win;
    WINDOW* table_win;
    WINDOW* footer_win;
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

void draw_header(WINDOW* win, const char* title);
void draw_footer(WINDOW* win, UIMode mode);
void draw_table(UIState* state);
void draw_table_row(WINDOW* win, int row, int col, Release* release, bool selected);
void update_display(UIState* state);

void handle_table_input(UIState* state, int ch);
void handle_input(UIState* state);

void center_text(WINDOW* win, int row, const char* text);
void set_loading_message(UIState* state, int row, const char* owner, const char* repo);

#endif // UI_H