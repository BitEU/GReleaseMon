#include "release_page.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_LINE_CAPACITY 100
#define MAX_LINE_LENGTH 1024

ReleasePage* create_release_page(Release* release) {
    ReleasePage* page = calloc(1, sizeof(ReleasePage));
    if (!page) return NULL;
    
    page->release = release;
    page->scroll_offset = 0;
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create window leaving space for header and footer
    page->window = newwin(max_y - 6, max_x - 4, 4, 2);
    page->window_height = max_y - 6;
    page->window_width = max_x - 4;
    
    // Initialize lines array
    page->line_capacity = INITIAL_LINE_CAPACITY;
    page->lines = calloc(page->line_capacity, sizeof(char*));
    if (!page->lines) {
        delwin(page->window);
        free(page);
        return NULL;
    }
    
    // Parse the release body into lines
    parse_release_body(page, release->body);
    
    return page;
}

void free_release_page(ReleasePage* page) {
    if (!page) return;
    
    if (page->lines) {
        for (int i = 0; i < page->line_count; i++) {
            if (page->lines[i]) {
                free(page->lines[i]);
            }
        }
        free(page->lines);
    }
    
    if (page->window) {
        delwin(page->window);
    }
    
    free(page);
}

void add_line_to_page(ReleasePage* page, const char* line) {
    if (page->line_count >= page->line_capacity) {
        int new_capacity = page->line_capacity * 2;
        char** new_lines = realloc(page->lines, new_capacity * sizeof(char*));
        if (!new_lines) return;
        
        page->lines = new_lines;
        page->line_capacity = new_capacity;
    }
    
    page->lines[page->line_count] = strdup(line);
    page->line_count++;
}

void wrap_and_add_line(ReleasePage* page, const char* text) {
    int text_len = strlen(text);
    int max_width = page->window_width - 4;  // Leave some margin
    
    if (text_len <= max_width) {
        add_line_to_page(page, text);
        return;
    }
    
    // Word wrap long lines
    char buffer[MAX_LINE_LENGTH];
    int start = 0;
    
    while (start < text_len) {
        int end = start + max_width;
        if (end >= text_len) {
            strncpy(buffer, text + start, text_len - start);
            buffer[text_len - start] = '\0';
            add_line_to_page(page, buffer);
            break;
        }
        
        // Find last space before max_width
        int last_space = end;
        while (last_space > start && text[last_space] != ' ') {
            last_space--;
        }
        
        if (last_space == start) {
            // No space found, break at max_width
            last_space = end;
        }
        
        strncpy(buffer, text + start, last_space - start);
        buffer[last_space - start] = '\0';
        add_line_to_page(page, buffer);
        
        start = last_space;
        if (text[start] == ' ') start++;  // Skip the space
    }
}

void parse_release_body(ReleasePage* page, const char* body) {
    if (!body) {
        add_line_to_page(page, "No release notes available.");
        return;
    }
    
    // Add header information
    char header[256];
    snprintf(header, sizeof(header), "Owner: %s", page->release->owner);
    add_line_to_page(page, header);
    
    snprintf(header, sizeof(header), "Repo: %s", page->release->repo);
    add_line_to_page(page, header);
    
    snprintf(header, sizeof(header), "Tag: %s", page->release->tag_name);
    add_line_to_page(page, header);
    
    // Format created_at
    struct tm* tm_info = localtime(&page->release->created_at);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(header, sizeof(header), "Created At: %s (%s)", 
             date_str, page->release->time_difference);
    add_line_to_page(page, header);
    
    add_line_to_page(page, "");
    add_line_to_page(page, "--- Release Notes ---");
    add_line_to_page(page, "");
    
    // Parse body line by line
    char* body_copy = strdup(body);
    char* line = strtok(body_copy, "\n");
    
    while (line != NULL) {
        // Handle empty lines
        if (strlen(line) == 0) {
            add_line_to_page(page, "");
        } else {
            wrap_and_add_line(page, line);
        }
        line = strtok(NULL, "\n");
    }
    
    free(body_copy);
}

void draw_release_content(ReleasePage* page) {
    werase(page->window);
    
    int visible_lines = page->window_height - 2;  // Account for borders
    int end_line = page->scroll_offset + visible_lines;
    if (end_line > page->line_count) {
        end_line = page->line_count;
    }
    
    int y = 1;
    for (int i = page->scroll_offset; i < end_line; i++) {
        // Simple color coding for headers
        if (strncmp(page->lines[i], "Owner:", 6) == 0 ||
            strncmp(page->lines[i], "Repo:", 5) == 0 ||
            strncmp(page->lines[i], "Tag:", 4) == 0 ||
            strncmp(page->lines[i], "Created At:", 11) == 0) {
            wattron(page->window, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
            mvwprintw(page->window, y, 2, "%s", page->lines[i]);
            wattroff(page->window, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
        } else if (strncmp(page->lines[i], "---", 3) == 0) {
            wattron(page->window, A_BOLD);
            mvwprintw(page->window, y, 2, "%s", page->lines[i]);
            wattroff(page->window, A_BOLD);
        } else {
            mvwprintw(page->window, y, 2, "%s", page->lines[i]);
        }
        y++;
    }
    
    // Draw scroll indicators
    if (page->scroll_offset > 0) {
        mvwprintw(page->window, 0, page->window_width / 2 - 4, " [MORE] ");
    }
    if (end_line < page->line_count) {
        mvwprintw(page->window, page->window_height - 1, 
                  page->window_width / 2 - 4, " [MORE] ");
    }
    
    box(page->window, 0, 0);
    wrefresh(page->window);
}

void scroll_release_page(ReleasePage* page, int direction) {
    int visible_lines = page->window_height - 2;
    
    if (direction > 0) {  // Scroll down
        if (page->scroll_offset + visible_lines < page->line_count) {
            page->scroll_offset++;
        }
    } else {  // Scroll up
        if (page->scroll_offset > 0) {
            page->scroll_offset--;
        }
    }
}

void display_release_page(ReleasePage* page, UIState* state) {
    draw_header(state->header_win, "Release Notes");
    draw_footer(state->footer_win, MODE_RELEASE_PAGE);
    draw_release_content(page);
}

void handle_release_input(ReleasePage* page, UIState* state, int ch) {
    switch (ch) {
        case 'j':
        case KEY_DOWN:
            scroll_release_page(page, 1);
            draw_release_content(page);
            break;
            
        case 'k':
        case KEY_UP:
            scroll_release_page(page, -1);
            draw_release_content(page);
            break;
            
        case 'G':
            // Go to bottom
            page->scroll_offset = page->line_count - (page->window_height - 2);
            if (page->scroll_offset < 0) page->scroll_offset = 0;
            draw_release_content(page);
            break;
            
        case 'g':
            // Go to top
            page->scroll_offset = 0;
            draw_release_content(page);
            break;
            
        case 'b':
        case 'B':
            // Go back to table view
            state->current_mode = MODE_TABLE;
            break;
    }
}