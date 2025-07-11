#ifdef _WIN32
#include <time.h>
#ifndef _mkgmtime
// Provide prototype for _mkgmtime if not included
time_t _mkgmtime(struct tm *tm);
#endif
#else
#include <time.h>
#endif
#ifndef REQUESTS_H
#define REQUESTS_H

#include <time.h>
#include <stdbool.h>
#include <Windows.h>
#include "config.h"

#define MAX_URL_LENGTH 512
#define MAX_TAG_LENGTH 128
#define MAX_TIME_DIFF_LENGTH 64

typedef struct {
    char owner[MAX_REPO_NAME_LENGTH];
    char repo[MAX_REPO_NAME_LENGTH];
    char tag_name[MAX_TAG_LENGTH];
    char url[MAX_URL_LENGTH];
    char* body;  // Dynamically allocated
    bool prerelease;
    time_t created_at;
    char time_difference[MAX_TIME_DIFF_LENGTH];
    bool has_windows_assets;
} Release;

typedef struct {
    Release* releases;
    int count;
    int capacity;
    CRITICAL_SECTION mutex;
} ReleaseCollection;

typedef struct {
    RepoInfo repo;
    ReleaseCollection* collection;
    const char* auth_token;
} FetchThreadData;

// Function declarations
ReleaseCollection* create_release_collection(int initial_capacity);
void free_release_collection(ReleaseCollection* collection);
void fetch_latest_release(RepoInfo* repo, ReleaseCollection* collection, const char* auth_token);
unsigned __stdcall fetch_release_thread(void* arg);
void calculate_time_diff(Release* release);
bool add_release_to_collection(ReleaseCollection* collection, Release* release);
void sort_releases_by_date(ReleaseCollection* collection);

// JSON parsing functions
char* extract_json_string(const char* json, const char* key);
bool extract_json_bool(const char* json, const char* key);

// Windows assets detection
bool is_windows_asset(const char* name);
bool check_windows_assets(const char* json);

#endif // REQUESTS_H