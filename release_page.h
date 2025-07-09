#ifndef RELEASE_PAGE_H
#define RELEASE_PAGE_H

#include <curses.h>
#include "requests.h"
#include "ui.h"

typedef struct {
    WINDOW* window;
    Release* release;
    char** lines;  // Array of text lines
    int line_count;
    int line_capacity;
    int scroll_offset;
    int window_height;
    int window_width;
} ReleasePage;

// Forward declaration to avoid circular dependency
struct UIState;

// Function declarations
ReleasePage* create_release_page(Release* release);
void free_release_page(ReleasePage* page);
void display_release_page(ReleasePage* page, struct UIState* state);
void handle_release_input(ReleasePage* page, struct UIState* state, int ch);
void scroll_release_page(ReleasePage* page, int direction);
void parse_release_body(ReleasePage* page, const char* body);
void draw_release_content(ReleasePage* page);

#endif // RELEASE_PAGE_H