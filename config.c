#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

char* get_config_path(void) {
    static char path[MAX_PATH_LENGTH];
    char* userprofile = getenv("USERPROFILE");
    
    if (!userprofile) {
        fprintf(stderr, "Error: USERPROFILE environment variable not set\n");
        return NULL;
    }
    
    snprintf(path, sizeof(path), "%s\\.config\\nodebro\\config.txt", userprofile);
    return path;
}

Config* load_config(const char* path) {
    FILE* fp;
    char line[1024];
    char key[256];
    char value[768];
    int repo_count = 0;
    
    Config* config = calloc(1, sizeof(Config));
    if (!config) {
        fprintf(stderr, "Error: Failed to allocate memory for config\n");
        return NULL;
    }
    
    // Open config file
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open config file: %s\n", path);
        free(config);
        return NULL;
    }
    
    // First pass: count repositories
    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#' || line[0] == '\0') continue;
        
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        
        // Parse key=value format
        if (sscanf(line, "%255[^=]=%767[^\n]", key, value) == 2) {
            // Remove leading/trailing whitespace from key
            char* key_start = key;
            while (*key_start == ' ' || *key_start == '\t') key_start++;
            char* key_end = key_start + strlen(key_start) - 1;
            while (key_end > key_start && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';
            
            if (strncmp(key_start, "repo", 4) == 0) {
                repo_count++;
            }
        }
    }
    
    if (repo_count == 0) {
        fprintf(stderr, "Error: No repositories found in config\n");
        fclose(fp);
        free(config);
        return NULL;
    }
    
    // Allocate memory for repos
    config->repos = calloc(repo_count, sizeof(RepoInfo));
    if (!config->repos) {
        fprintf(stderr, "Error: Failed to allocate memory for repos\n");
        fclose(fp);
        free(config);
        return NULL;
    }
    
    config->repo_capacity = repo_count;
    
    // Second pass: parse configuration
    rewind(fp);
    int current_repo = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#' || line[0] == '\0') continue;
        
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        
        // Parse key=value format
        if (sscanf(line, "%255[^=]=%767[^\n]", key, value) == 2) {
            // Remove leading/trailing whitespace from key
            char* key_start = key;
            while (*key_start == ' ' || *key_start == '\t') key_start++;
            char* key_end = key_start + strlen(key_start) - 1;
            while (key_end > key_start && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';
            
            // Remove leading/trailing whitespace from value
            char* value_start = value;
            while (*value_start == ' ' || *value_start == '\t') value_start++;
            char* value_end = value_start + strlen(value_start) - 1;
            while (value_end > value_start && (*value_end == ' ' || *value_end == '\t')) *value_end-- = '\0';
            
            if (strcmp(key_start, "pat") == 0) {
                strncpy(config->pat, value_start, MAX_TOKEN_LENGTH - 1);
                config->pat[MAX_TOKEN_LENGTH - 1] = '\0';
            } else if (strncmp(key_start, "repo", 4) == 0 && current_repo < repo_count) {
                // Format: repo1=owner/repo or repo_1=owner/repo
                char* slash = strchr(value_start, '/');
                if (slash) {
                    *slash = '\0';
                    strncpy(config->repos[current_repo].owner, value_start, MAX_REPO_NAME_LENGTH - 1);
                    strncpy(config->repos[current_repo].repo, slash + 1, MAX_REPO_NAME_LENGTH - 1);
                    config->repos[current_repo].owner[MAX_REPO_NAME_LENGTH - 1] = '\0';
                    config->repos[current_repo].repo[MAX_REPO_NAME_LENGTH - 1] = '\0';
                    
                    printf("Loading Config: Owner: %s, Repo: %s\n", 
                           config->repos[current_repo].owner, config->repos[current_repo].repo);
                    current_repo++;
                } else {
                    fprintf(stderr, "Error: Invalid repo format: %s (expected owner/repo)\n", value_start);
                }
            }
        }
    }
    
    config->repo_count = current_repo;
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