#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

char* get_config_path(void) {
    static char path[MAX_PATH_LENGTH];
    HMODULE hModule = GetModuleHandle(NULL);
    if (hModule == NULL) {
        fprintf(stderr, "Error: Could not get module handle.\n");
        return NULL;
    }

    // Get the path of the executable
    DWORD length = GetModuleFileNameA(hModule, path, MAX_PATH_LENGTH);
    if (length == 0 || length == MAX_PATH_LENGTH) {
        fprintf(stderr, "Error: Could not get module file name.\n");
        return NULL;
    }

    // Find the last backslash (directory separator)
    char* last_backslash = strrchr(path, '\\');
    if (last_backslash != NULL) {
        // Null-terminate the string after the last backslash to get the directory path
        *(last_backslash + 1) = '\0';
    } else {
        // If no backslash, it's just the executable name, so use current directory
        path[0] = '.';
        path[1] = '\\';
        path[2] = '\0';
    }

    // Append the config file name
    strncat(path, "config.txt", MAX_PATH_LENGTH - strlen(path) - 1);
    return path;
}

Config* load_config(const char* path) {
    FILE* fp;
    char line[1024];
    
    Config* config = calloc(1, sizeof(Config));
    if (!config) {
        fprintf(stderr, "Error: Failed to allocate memory for config\n");
        return NULL;
    }
    
    config->repo_capacity = 10; // Initial capacity
    config->repos = calloc(config->repo_capacity, sizeof(RepoInfo));
    if (!config->repos) {
        fprintf(stderr, "Error: Failed to allocate memory for repos\n");
        free(config);
        return NULL;
    }
    
    // Open config file
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open config file: %s\n", path);
        free(config->repos);
        free(config);
        return NULL;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        
        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#') continue;
        
        // Check for PAT token
        if (strncmp(line, "pat=", 4) == 0) {
            strncpy(config->pat, line + 4, MAX_TOKEN_LENGTH - 1);
            config->pat[MAX_TOKEN_LENGTH - 1] = '\0';
        } else {
            // Assume it's a repository line (owner/repo)
            char* slash = strchr(line, '/');
            if (slash) {
                if (config->repo_count >= config->repo_capacity) {
                    config->repo_capacity *= 2;
                    RepoInfo* new_repos = realloc(config->repos, config->repo_capacity * sizeof(RepoInfo));
                    if (!new_repos) {
                        fprintf(stderr, "Error: Failed to reallocate memory for repos\n");
                        fclose(fp);
                        free_config(config);
                        return NULL;
                    }
                    config->repos = new_repos;
                }
                
                *slash = '\0';
                strncpy(config->repos[config->repo_count].owner, line, MAX_REPO_NAME_LENGTH - 1);
                strncpy(config->repos[config->repo_count].repo, slash + 1, MAX_REPO_NAME_LENGTH - 1);
                config->repos[config->repo_count].owner[MAX_REPO_NAME_LENGTH - 1] = '\0';
                config->repos[config->repo_count].repo[MAX_REPO_NAME_LENGTH - 1] = '\0';
                
                printf("Loading Config: Owner: %s, Repo: %s\n", 
                       config->repos[config->repo_count].owner, config->repos[config->repo_count].repo);
                config->repo_count++;
            } else {
                fprintf(stderr, "Warning: Invalid line in config (expected owner/repo or pat=): %s\n", line);
            }
        }
    }
    
    fclose(fp);
    return config;
}

void free_config(Config* config) {
    if (!config) return;
    
    if (config->repos) {
        free(config->repos);
    }
    
    free(config);
}

bool validate_config(const Config* config) {
    if (!config) return false;
    
    if (strlen(config->pat) == 0) {
        fprintf(stderr, "Error: PAT token is empty\n");
        return false;
    }
    
    if (config->repo_count == 0) {
        fprintf(stderr, "Error: No repositories configured\n");
        return false;
    }
    
    return true;
}