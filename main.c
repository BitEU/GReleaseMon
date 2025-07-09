#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <curl/curl.h>
#include "config.h"
#include "requests.h"
#include "ui.h"
#include "release_page.h"
#include "utils.h"

// Global variables
static volatile bool g_running = true;
static UIState* g_ui_state = NULL;
static ReleasePage* g_current_release_page = NULL;

// Signal handler for clean shutdown
void signal_handler(int sig) {
    g_running = false;
}

// Thread function for updating the display periodically
void* update_thread(void* arg) {
    UIState* state = (UIState*)arg;
    
    while (g_running) {
        // Update the display every 500ms if new data is available
        msleep(500);
        
        if (state->current_mode == MODE_TABLE) {
            // Check if we need to resort
            static int last_count = 0;
            pthread_mutex_lock(&state->releases->mutex);
            int current_count = state->releases->count;
            pthread_mutex_unlock(&state->releases->mutex);
            
            if (current_count != last_count) {
                sort_releases_by_date(state->releases);
                update_display(state);
                last_count = current_count;
            }
        }
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    ErrorCode error = SUCCESS;
    Config* config = NULL;
    ReleaseCollection* releases = NULL;
    pthread_t* fetch_threads = NULL;
    pthread_t update_thread_id;
    FetchThreadData* thread_data = NULL;
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize CURL globally
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        handle_error(ERROR_CURL_INIT, "Failed to initialize CURL library");
        return ERROR_CURL_INIT;
    }
    
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
    fetch_threads = calloc(config->repo_count, sizeof(pthread_t));
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
        
        if (pthread_create(&fetch_threads[i], NULL, fetch_release_thread, &thread_data[i]) != 0) {
            log_message("Failed to create thread for %s/%s", 
                       config->repos[i].owner, config->repos[i].repo);
        }
    }
    
    // Start update thread
    pthread_create(&update_thread_id, NULL, update_thread, g_ui_state);
    
    // Main event loop
    while (g_running) {
        int ch = getch();
        
        // Global key handlers
        if (ch == 'x' || ch == 'X') {
            g_running = false;
            break;
        }
        
        switch (g_ui_state->current_mode) {
            case MODE_TABLE:
                handle_table_input(g_ui_state, ch);
                
                // Check if we need to show release page
                if (g_ui_state->current_mode == MODE_RELEASE_PAGE) {
                    pthread_mutex_lock(&releases->mutex);
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
                    pthread_mutex_unlock(&releases->mutex);
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
    
    // Cancel update thread
    pthread_cancel(update_thread_id);
    pthread_join(update_thread_id, NULL);
    
    // Wait for all fetch threads to complete
    for (int i = 0; i < config->repo_count; i++) {
        pthread_join(fetch_threads[i], NULL);
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
    
    // Cleanup CURL
    curl_global_cleanup();
    
    if (error != SUCCESS) {
        fprintf(stderr, "\nPress any key to exit...\n");
        getchar();
    }
    
    return error;
}