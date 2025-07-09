#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <conio.h>

void setup_windows_console(void) {
    // Enable UTF-8 support
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Enable virtual terminal processing for better color support
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    
    // Set console title
    SetConsoleTitle("NodeBro - GitHub Release Tracker");
}

void init_ui(void) {
    setup_windows_console();
    
    // Hide cursor
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &cursorInfo);
    
    // Clear screen
    system("cls");
}

void cleanup_ui(void) {
    // Show cursor
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &cursorInfo);
    
    // Reset colors
    SetConsoleTextAttribute(hOut, CONSOLE_COLOR_NORMAL);
}

void setup_colors(void) {
    // Colors are handled per-print in Windows Console API
    // No global color initialization needed
}

UIState* create_ui_state(Config* config, ReleaseCollection* releases) {
    UIState* state = calloc(1, sizeof(UIState));
    if (!state) return NULL;
    
    state->hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(state->hConsole, &state->csbi);
    
    state->console_width = state->csbi.srWindow.Right - state->csbi.srWindow.Left + 1;
    state->console_height = state->csbi.srWindow.Bottom - state->csbi.srWindow.Top + 1;
    
    state->selected_row = 1;
    state->table_start_row = 0;
    state->visible_rows = state->console_height - 7; // Leave space for header, table header, and footer
    state->total_rows = 0;
    state->current_mode = MODE_TABLE;
    state->releases = releases;
    state->config = config;
    
    return state;
}

void free_ui_state(UIState* state) {
    if (state) {
        free(state);
    }
}

void clear_console(UIState* state) {
    system("cls");
}

void set_console_cursor_position(UIState* state, int x, int y) {
    COORD pos = {x, y};
    SetConsoleCursorPosition(state->hConsole, pos);
}

void set_console_color(UIState* state, WORD color) {
    SetConsoleTextAttribute(state->hConsole, color);
}

void print_at(UIState* state, int x, int y, const char* text) {
    set_console_cursor_position(state, x, y);
    printf("%s", text);
}

void print_colored_at(UIState* state, int x, int y, const char* text, WORD color) {
    set_console_cursor_position(state, x, y);
    set_console_color(state, color);
    printf("%s", text);
    set_console_color(state, CONSOLE_COLOR_NORMAL);
}

int getch(void) {
    return _getch();
}

void draw_header(UIState* state, const char* title) {
    char header[256];
    snprintf(header, sizeof(header), "=== %s ===", title);
    
    // Calculate center position
    int center_x = (state->console_width - strlen(header)) / 2;
    print_colored_at(state, center_x, 0, header, CONSOLE_COLOR_HEADER);
    
    // Draw separator line
    for (int i = 0; i < state->console_width; i++) {
        print_colored_at(state, i, 1, "-", CONSOLE_COLOR_HEADER);
    }
}

void draw_footer(UIState* state, UIMode mode) {
    int footer_y = state->console_height - 2;
    
    // Draw separator line
    for (int i = 0; i < state->console_width; i++) {
        print_colored_at(state, i, footer_y, "-", CONSOLE_COLOR_HEADER);
    }
    
    const char* help_text = "";
    switch (mode) {
        case MODE_TABLE:
            help_text = "Arrow keys: Navigate | Enter: View release | X: Exit";
            break;
        case MODE_RELEASE_PAGE:
            help_text = "Arrow keys: Scroll | Esc: Back to table | X: Exit";
            break;
        default:
            help_text = "X: Exit";
            break;
    }
    
    print_at(state, 2, footer_y + 1, help_text);
}

void draw_table_row(UIState* state, int row, Release* release, bool selected) {
    char line[1024];
    memset(line, 0, sizeof(line)); // Clear the buffer
    WORD color = selected ? CONSOLE_COLOR_SELECTED : CONSOLE_COLOR_NORMAL;
    
    // Determine color based on release age
    if (!selected) {
        if (strstr(release->time_difference, "h ago")) {
            color = CONSOLE_COLOR_FRESH;
        } else if (strstr(release->time_difference, "1d ago")) {
            color = CONSOLE_COLOR_DAY_OLD;
        }
    }
    
    // Format: Owner/Repo | Tag | Time | Prerelease | Windows Assets
    char repo_full[64];
    snprintf(repo_full, sizeof(repo_full), "%s/%s", release->owner, release->repo);
    snprintf(line, sizeof(line), "%-20s | %-15s | %-10s | %-4s | %s",
             repo_full, release->tag_name, release->time_difference,
             release->prerelease ? "Pre" : "",
             release->has_windows_assets ? "Yes" : "No");
    
    // Truncate if too long
    if (strlen(line) > state->console_width - 2) {
        line[state->console_width - 2] = '\0';
    }
    
    // Clear the line first
    char clear_line[1024];
    memset(clear_line, ' ', state->console_width - 2);
    clear_line[state->console_width - 2] = '\0';
    print_at(state, 1, row + 4, clear_line);
    
    // Then print the actual content
    print_colored_at(state, 1, row + 4, line, color);
}

void draw_table(UIState* state) {
    EnterCriticalSection(&state->releases->mutex);
    
    // Draw table header
    print_colored_at(state, 1, 3, "Repository           | Tag             | Time       | Type | Windows", CONSOLE_COLOR_HEADER);
    
    // Draw releases
    int visible_count = 0;
    for (int i = state->table_start_row; i < state->releases->count && visible_count < state->visible_rows; i++) {
        bool selected = (i + 1 == state->selected_row);
        draw_table_row(state, visible_count, &state->releases->releases[i], selected);
        visible_count++;
    }
    
    // Clear remaining lines
    for (int i = visible_count; i < state->visible_rows; i++) {
        char empty_line[512];
        memset(empty_line, ' ', state->console_width - 2);
        empty_line[state->console_width - 2] = '\0';
        print_at(state, 1, i + 5, empty_line);
    }
    
    state->total_rows = state->releases->count;
    LeaveCriticalSection(&state->releases->mutex);
}

void update_display(UIState* state) {
    clear_console(state);
    
    switch (state->current_mode) {
        case MODE_TABLE:
            draw_header(state, "GitHub Release Monitor");
            draw_table(state);
            draw_footer(state, MODE_TABLE);
            break;
        case MODE_RELEASE_PAGE:
            // Release page display is handled by display_release_page()
            draw_footer(state, MODE_RELEASE_PAGE);
            break;
        default:
            break;
    }
    
    // Force flush
    fflush(stdout);
}

void center_text(UIState* state, int row, const char* text) {
    int center_x = (state->console_width - strlen(text)) / 2;
    print_at(state, center_x, row, text);
}

void set_loading_message(UIState* state, int row, const char* owner, const char* repo) {
    char message[256];
    snprintf(message, sizeof(message), "Loading %s/%s...", owner, repo);
    center_text(state, row, message);
}

void handle_table_input(UIState* state, int ch) {
    switch (ch) {
        case 72: // Up arrow (Windows)
        case 'k':
            if (state->selected_row > 1) {
                state->selected_row--;
                if (state->selected_row <= state->table_start_row) {
                    state->table_start_row--;
                }
            }
            break;
            
        case 80: // Down arrow (Windows)
        case 'j':
            if (state->selected_row < state->total_rows) {
                state->selected_row++;
                if (state->selected_row > state->table_start_row + state->visible_rows) {
                    state->table_start_row++;
                }
            }
            break;
            
        case 13: // Enter
            if (state->selected_row > 0 && state->selected_row <= state->total_rows) {
                state->current_mode = MODE_RELEASE_PAGE;
            }
            break;
    }
    
    update_display(state);
}

void handle_input(UIState* state) {
    int ch = getch();
    
    switch (state->current_mode) {
        case MODE_TABLE:
            handle_table_input(state, ch);
            break;
        case MODE_RELEASE_PAGE:
            // Handle release page input
            if (ch == 27) { // Escape
                state->current_mode = MODE_TABLE;
                update_display(state);
            }
            break;
        default:
            break;
    }
}