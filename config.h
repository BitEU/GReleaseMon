#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define MAX_PATH_LENGTH 512
#define MAX_TOKEN_LENGTH 256
#define MAX_REPO_NAME_LENGTH 128

typedef struct {
    char owner[MAX_REPO_NAME_LENGTH];
    char repo[MAX_REPO_NAME_LENGTH];
} RepoInfo;

typedef struct {
    char pat[MAX_TOKEN_LENGTH];
    RepoInfo* repos;
    int repo_count;
    int repo_capacity;
} Config;

// Function declarations
Config* load_config(const char* path);
void free_config(Config* config);
char* get_config_path(void);
bool validate_config(const Config* config);

#endif // CONFIG_H
