#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include "toml.h"

char* get_config_path(void) {
    static char path[MAX_PATH_LENGTH];
    char* userprofile = getenv("USERPROFILE");
    
    if (!userprofile) {
        fprintf(stderr, "Error: USERPROFILE environment variable not set\n");
        return NULL;
    }
    
    snprintf(path, sizeof(path), "%s\\.config\\nodebro\\config", userprofile);
    return path;
}

Config* load_config(const char* path) {
    FILE* fp;
    toml_table_t* conf;
    toml_array_t* repos_array;
    char errbuf[256];
    
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
    
    // Parse TOML
    conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    
    if (!conf) {
        fprintf(stderr, "Error: Cannot parse config file: %s\n", errbuf);
        free(config);
        return NULL;
    }
    
    // Get PAT token
    toml_datum_t pat = toml_string_in(conf, "pat");
    if (!pat.ok) {
        fprintf(stderr, "Error: Missing 'pat' in config file\n");
        toml_free(conf);
        free(config);
        return NULL;
    }
    
    strncpy(config->pat, pat.u.s, MAX_TOKEN_LENGTH - 1);
    config->pat[MAX_TOKEN_LENGTH - 1] = '\0';
    free(pat.u.s);
    
    // Get repos array
    repos_array = toml_array_in(conf, "repos");
    if (!repos_array) {
        fprintf(stderr, "Error: Missing 'repos' array in config file\n");
        toml_free(conf);
        free(config);
        return NULL;
    }
    
    int repo_count = toml_array_nelem(repos_array);
    config->repo_count = repo_count;
    config->repo_capacity = repo_count;
    config->repos = calloc(repo_count, sizeof(RepoInfo));
    
    if (!config->repos) {
        fprintf(stderr, "Error: Failed to allocate memory for repos\n");
        toml_free(conf);
        free(config);
        return NULL;
    }
    
    // Parse each repo
    for (int i = 0; i < repo_count; i++) {
        toml_table_t* repo_table = toml_table_at(repos_array, i);
        if (!repo_table) {
            fprintf(stderr, "Error: Invalid repo entry at index %d\n", i);
            continue;
        }
        
        toml_datum_t owner = toml_string_in(repo_table, "Owner");
        toml_datum_t repo = toml_string_in(repo_table, "Repo");
        
        if (!owner.ok || !repo.ok) {
            fprintf(stderr, "Error: Missing Owner or Repo in config at index %d\n", i);
            if (owner.ok) free(owner.u.s);
            if (repo.ok) free(repo.u.s);
            continue;
        }
        
        strncpy(config->repos[i].owner, owner.u.s, MAX_REPO_NAME_LENGTH - 1);
        strncpy(config->repos[i].repo, repo.u.s, MAX_REPO_NAME_LENGTH - 1);
        config->repos[i].owner[MAX_REPO_NAME_LENGTH - 1] = '\0';
        config->repos[i].repo[MAX_REPO_NAME_LENGTH - 1] = '\0';
        
        free(owner.u.s);
        free(repo.u.s);
        
        printf("Loading Config: Owner: %s, Repo: %s\n", 
               config->repos[i].owner, config->repos[i].repo);
    }
    
    toml_free(conf);
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