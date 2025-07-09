#ifndef REQUESTS_H
#define REQUESTS_H

#include <time.h>
#include <stdbool.h>
#include <pthread.h>
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
} Release;

typedef struct {
    Release* releases;
    int count;
    int capacity;
    pthread_mutex_t mutex;
} ReleaseCollection;

typedef struct {
    RepoInfo repo;
    ReleaseCollection* collection;
    const char* auth_token;
} FetchThreadData;

// Memory buffer for curl
typedef struct {
    char* memory;
    size_t size;
} MemoryStruct;

// Function declarations
ReleaseCollection* create_release_collection(int initial_capacity);
void free_release_collection(ReleaseCollection* collection);
void fetch_latest_release(RepoInfo* repo, ReleaseCollection* collection, const char* auth_token);
void* fetch_release_thread(void* arg);
void calculate_time_diff(Release* release);
bool add_release_to_collection(ReleaseCollection* collection, Release* release);
void sort_releases_by_date(ReleaseCollection* collection);

// CURL callback
size_t write_memory_callback(void* contents, size_t size, size_t nmemb, void* userp);

#endif // REQUESTS_H