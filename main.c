#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <conio.h>
#include "config.h"
#include "requests.h"
#include "ui.h"
#include "release_page.h"
#include "utils.h"

// Global variables
static volatile bool g_running = true;
static UIState* g_ui_state = NULL;
static ReleasePage* g_current_release_page = NULL;

// Console control handler for clean shutdown
BOOL WINAPI console_handler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            g_running = false;
            return TRUE;
        default:
            return FALSE;
    }
}

// Thread function for updating the display periodically
unsigned __stdcall update_thread(void* arg) {
    UIState* state = (UIState*)arg;
    
    while (g_running) {
        // Update the display every 500ms if new data is available
        msleep(500);
        
        if (state->current_mode == MODE_TABLE) {
            // Check if we need to resort
            static int last_count = 0;
            EnterCriticalSection(&state->releases->mutex);
            int current_count = state->releases->count;
            LeaveCriticalSection(&state->releases->mutex);
            
            if (current_count != last_count) {
                sort_releases_by_date(state->releases);
                update_display(state);
                last_count = current_count;
            }
        }
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    ErrorCode error = SUCCESS;
    Config* config = NULL;
    ReleaseCollection* releases = NULL;
    HANDLE* fetch_threads = NULL;
    HANDLE update_thread_handle;
    FetchThreadData* thread_data = NULL;
    
    // Set up console control handler
    SetConsoleCtrlHandler(console_handler, TRUE);
    
    // Initialize WinHTTP (no global init needed)
    
    // Load configuration
    char* config_path = get_config_path();
    if (!config_path) {
        error = ERROR_CONFIG_NOT_FOUND;
        goto cleanup;
    }
    
    printf("Loading configuration from: %s\n", config_path);
    config = load_config(config_path);
    if (!config) {
        error = ERROR_CONFIG_NOT_FOUND;
        goto cleanup;
    }
    
    if (!validate_config(config)) {
        error = ERROR_CONFIG_INVALID;
        goto cleanup;
    }
    
    // Create release collection
    releases = create_release_collection(config->repo_count);
    if (!releases) {
        error = ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    // Initialize UI
    init_ui();
    g_ui_state = create_ui_state(config, releases);
    if (!g_ui_state) {
        cleanup_ui();
        error = ERROR_UI_INIT;
        goto cleanup;
    }
    
    // Initial display
    update_display(g_ui_state);
    
    // Allocate memory for threads
    fetch_threads = calloc(config->repo_count, sizeof(HANDLE));
    thread_data = calloc(config->repo_count, sizeof(FetchThreadData));
    if (!fetch_threads || !thread_data) {
        error = ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    // Start fetching releases in parallel
    for (int i = 0; i < config->repo_count; i++) {
        thread_data[i].repo = config->repos[i];
        thread_data[i].collection = releases;
        thread_data[i].auth_token = config->pat;
        
        fetch_threads[i] = (HANDLE)_beginthreadex(NULL, 0, fetch_release_thread, &thread_data[i], 0, NULL);
        if (fetch_threads[i] == 0) {
            log_message("Failed to create thread for %s/%s", 
                       config->repos[i].owner, config->repos[i].repo);
        }
    }
    
    // Start update thread
    update_thread_handle = (HANDLE)_beginthreadex(NULL, 0, update_thread, g_ui_state, 0, NULL);
    
    // Main event loop
    while (g_running) {
        if (_kbhit()) {
            int ch = getch();
            
            // Global key handlers
            if (ch == 'x' || ch == 'X' || ch == 17) { // 17 = Ctrl+Q
                g_running = false;
                break;
            }
        
        switch (g_ui_state->current_mode) {
            case MODE_TABLE:
                handle_table_input(g_ui_state, ch);
                
                // Check if we need to show release page
                if (g_ui_state->current_mode == MODE_RELEASE_PAGE) {
                    EnterCriticalSection(&releases->mutex);
                    if (g_ui_state->selected_row > 0 && 
                        g_ui_state->selected_row <= releases->count) {
                        Release* selected = &releases->releases[g_ui_state->selected_row - 1];
                        
                        // Clean up old release page
                        if (g_current_release_page) {
                            free_release_page(g_current_release_page);
                        }
                        
                        // Create new release page
                        g_current_release_page = create_release_page(selected);
                        if (g_current_release_page) {
                            display_release_page(g_current_release_page, g_ui_state);
                        } else {
                            g_ui_state->current_mode = MODE_TABLE;
                        }
                    }
                    LeaveCriticalSection(&releases->mutex);
                }
                break;
                
            case MODE_RELEASE_PAGE:
                if (g_current_release_page) {
                    handle_release_input(g_current_release_page, g_ui_state, ch);
                    
                    // Check if we need to go back to table
                    if (g_ui_state->current_mode == MODE_TABLE) {
                        free_release_page(g_current_release_page);
                        g_current_release_page = NULL;
                        update_display(g_ui_state);
                    }
                }
                break;
        }
        
        // Small delay to prevent high CPU usage
        msleep(10);
    }
    
    // Wait for update thread to complete
    WaitForSingleObject(update_thread_handle, INFINITE);
    CloseHandle(update_thread_handle);
    
    // Wait for all fetch threads to complete
    for (int i = 0; i < config->repo_count; i++) {
        if (fetch_threads[i]) {
            WaitForSingleObject(fetch_threads[i], INFINITE);
            CloseHandle(fetch_threads[i]);
        }
    }
    
cleanup:
    // Clean up UI
    if (g_current_release_page) {
        free_release_page(g_current_release_page);
    }
    if (g_ui_state) {
        free_ui_state(g_ui_state);
    }
    cleanup_ui();
    
    // Free resources
    if (thread_data) free(thread_data);
    if (fetch_threads) free(fetch_threads);
    if (releases) free_release_collection(releases);
    if (config) free_config(config);
    
    // Cleanup WinHTTP (no global cleanup needed)
    
    if (error != SUCCESS) {
        fprintf(stderr, "\nPress any key to exit...\n");
        getchar();
    }
    
    return error;
}