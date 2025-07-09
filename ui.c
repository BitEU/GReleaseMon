#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <regex.h>

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
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  // Hide cursor
    
    if (has_colors()) {
        setup_colors();
    }
    
    clear();
    refresh();
}

void cleanup_ui(void) {
    endwin();
}

void setup_colors(void) {
    start_color();
    
    // Define color pairs
    init_pair(COLOR_PAIR_NORMAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_HEADER, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_PAIR_SELECTED, COLOR_BLACK, COLOR_CYAN);
    init_pair(COLOR_PAIR_FRESH, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_PAIR_DAY_OLD, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_PAIR_ERROR, COLOR_RED, COLOR_BLACK);
}

UIState* create_ui_state(Config* config, ReleaseCollection* releases) {
    UIState* state = calloc(1, sizeof(UIState));
    if (!state) return NULL;
    
    state->config = config;
    state->releases = releases;
    state->selected_row = 1;  // Start at first data row (skip header)
    state->table_start_row = 0;
    state->current_mode = MODE_TABLE;
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create windows
    state->header_win = newwin(3, max_x, 0, 0);
    state->footer_win = newwin(3, max_x, max_y - 3, 0);
    state->table_win = newwin(max_y - 6, max_x, 3, 0);
    
    state->visible_rows = max_y - 6 - 2;  // Subtract borders
    state->total_rows = config->repo_count + 1;  // +1 for header
    
    // Enable scrolling for table window
    scrollok(state->table_win, TRUE);
    
    return state;
}

void free_ui_state(UIState* state) {
    if (!state) return;
    
    if (state->header_win) delwin(state->header_win);
    if (state->table_win) delwin(state->table_win);
    if (state->footer_win) delwin(state->footer_win);
    
    free(state);
}

void center_text(WINDOW* win, int row, const char* text) {
    int max_x = getmaxx(win);
    int len = strlen(text);
    int start_x = (max_x - len) / 2;
    if (start_x < 0) start_x = 0;
    mvwprintw(win, row, start_x, "%s", text);
}

void draw_header(WINDOW* win, const char* title) {
    werase(win);
    box(win, 0, 0);
    
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    center_text(win, 1, title);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    
    wrefresh(win);
}

void draw_footer(WINDOW* win, UIMode mode) {
    werase(win);
    box(win, 0, 0);
    
    const char* help_text;
    switch (mode) {
        case MODE_TABLE:
            help_text = "(j/↓) Down  (k/↑) Up  (Enter) View  (x) Exit";
            break;
        case MODE_RELEASE_PAGE:
            help_text = "(j) Scroll Down  (k) Scroll Up  (b) Back  (x) Exit";
            break;
        default:
            help_text = "(x) Exit";
    }
    
    center_text(win, 1, help_text);
    wrefresh(win);
}

void draw_table_row(WINDOW* win, int row, int col, Release* release, bool selected) {
    if (selected) {
        wattron(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
    }
    
    // Draw columns with proper spacing
    mvwprintw(win, row, col + 2, "%-20.20s", release->owner);
    mvwprintw(win, row, col + 25, "%-25.25s", release->repo);
    
    // Format date
    char date_str[20];
    struct tm* tm_info = localtime(&release->created_at);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M", tm_info);
    mvwprintw(win, row, col + 52, "%-18s", date_str);
    
    // Time difference with color
    if (!selected) {
        regex_t regex_hour, regex_day;
        regcomp(&regex_hour, "[0-9]+h ago", REG_EXTENDED);
        regcomp(&regex_day, "[0-9]+d ago", REG_EXTENDED);
        
        if (regexec(&regex_hour, release->time_difference, 0, NULL, 0) == 0) {
            wattron(win, COLOR_PAIR(COLOR_PAIR_FRESH));
        } else if (regexec(&regex_day, release->time_difference, 0, NULL, 0) == 0) {
            wattron(win, COLOR_PAIR(COLOR_PAIR_DAY_OLD));
        }
        
        regfree(&regex_hour);
        regfree(&regex_day);
    }
    
    mvwprintw(win, row, col + 72, "%-12s", release->time_difference);
    
    if (!selected) {
        wattroff(win, COLOR_PAIR(COLOR_PAIR_FRESH));
        wattroff(win, COLOR_PAIR(COLOR_PAIR_DAY_OLD));
    }
    
    // Tag name
    mvwprintw(win, row, col + 86, "%-20.20s", release->tag_name);
    
    if (selected) {
        wattroff(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
    }
}

void draw_table(UIState* state) {
    werase(state->table_win);
    box(state->table_win, 0, 0);
    
    int max_y, max_x;
    getmaxyx(state->table_win, max_y, max_x);
    
    // Draw table header
    wattron(state->table_win, A_BOLD | A_UNDERLINE);
    mvwprintw(state->table_win, 1, 2, "%-20s %-25s %-18s %-12s %-20s", 
              "Owner", "Repo", "Created At", "Time Ago", "Tag");
    wattroff(state->table_win, A_BOLD | A_UNDERLINE);
    
    // Draw horizontal line after header
    mvwhline(state->table_win, 2, 1, ACS_HLINE, max_x - 2);
    
    pthread_mutex_lock(&state->releases->mutex);
    
    // Draw visible rows
    int display_row = 3;
    int data_row = 0;
    
    for (int i = 0; i < state->releases->count && display_row < max_y - 1; i++) {
        if (data_row >= state->table_start_row) {
            bool selected = (data_row + 1 == state->selected_row);
            draw_table_row(state->table_win, display_row, 0, 
                         &state->releases->releases[i], selected);
            display_row++;
        }
        data_row++;
    }
    
    // Draw "Loading..." for repos without data yet
    for (int i = state->releases->count; i < state->config->repo_count && display_row < max_y - 1; i++) {
        if (data_row >= state->table_start_row) {
            bool selected = (data_row + 1 == state->selected_row);
            if (selected) {
                wattron(state->table_win, COLOR_PAIR(COLOR_PAIR_SELECTED));
            }
            
            mvwprintw(state->table_win, display_row, 2, "%-20.20s", state->config->repos[i].owner);
            mvwprintw(state->table_win, display_row, 25, "%-25.25s", state->config->repos[i].repo);
            mvwprintw(state->table_win, display_row, 52, "Fetching...");
            mvwprintw(state->table_win, display_row, 72, "Calculating...");
            mvwprintw(state->table_win, display_row, 86, "Fetching...");
            
            if (selected) {
                wattroff(state->table_win, COLOR_PAIR(COLOR_PAIR_SELECTED));
            }
            display_row++;
        }
        data_row++;
    }
    
    pthread_mutex_unlock(&state->releases->mutex);
    
    wrefresh(state->table_win);
}

void update_display(UIState* state) {
    switch (state->current_mode) {
        case MODE_TABLE:
            draw_header(state->header_win, "GitHub Releases Tracker - Latest Tags");
            draw_table(state);
            draw_footer(state->footer_win, MODE_TABLE);
            break;
        case MODE_RELEASE_PAGE:
            // Will be handled by release_page module
            break;
        case MODE_TAG_DROPDOWN:
            // Future implementation
            break;
    }
}

void handle_table_input(UIState* state, int ch) {
    int old_selected = state->selected_row;
    
    switch (ch) {
        case 'j':
        case KEY_DOWN:
            if (state->selected_row < state->total_rows - 1) {
                state->selected_row++;
                
                // Scroll down if needed
                if (state->selected_row - state->table_start_row >= state->visible_rows - 3) {
                    state->table_start_row++;
                }
            }
            break;
            
        case 'k':
        case KEY_UP:
            if (state->selected_row > 1) {
                state->selected_row--;
                
                // Scroll up if needed
                if (state->selected_row <= state->table_start_row + 1) {
                    state->table_start_row = state->selected_row - 1;
                    if (state->table_start_row < 0) state->table_start_row = 0;
                }
            }
            break;
            
        case '\n':
        case '\r':
        case KEY_ENTER:
            // Switch to release page mode
            if (state->selected_row > 0 && state->selected_row <= state->releases->count) {
                state->current_mode = MODE_RELEASE_PAGE;
            }
            break;
            
        case 'x':
        case 'X':
            // Exit will be handled in main loop
            break;
    }
    
    // Redraw if selection changed
    if (old_selected != state->selected_row) {
        draw_table(state);
    }
}

void handle_input(UIState* state) {
    int ch = getch();
    
    switch (state->current_mode) {
        case MODE_TABLE:
            handle_table_input(state, ch);
            break;
        case MODE_RELEASE_PAGE:
            // Will be handled by release_page module
            break;
        case MODE_TAG_DROPDOWN:
            // Future implementation
            break;
    }
}

void set_loading_message(UIState* state, int row, const char* owner, const char* repo) {
    pthread_mutex_lock(&state->releases->mutex);
    
    mvwprintw(state->table_win, row + 3, 2, "%-20.20s %-25.25s Fetching...", owner, repo);
    wrefresh(state->table_win);
    
    pthread_mutex_unlock(&state->releases->mutex);
}