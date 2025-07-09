#include "requests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"

size_t write_memory_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    MemoryStruct* mem = (MemoryStruct*)userp;
    
    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Error: Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

ReleaseCollection* create_release_collection(int initial_capacity) {
    ReleaseCollection* collection = calloc(1, sizeof(ReleaseCollection));
    if (!collection) return NULL;
    
    collection->releases = calloc(initial_capacity, sizeof(Release));
    if (!collection->releases) {
        free(collection);
        return NULL;
    }
    
    collection->capacity = initial_capacity;
    collection->count = 0;
    pthread_mutex_init(&collection->mutex, NULL);
    
    return collection;
}

void free_release_collection(ReleaseCollection* collection) {
    if (!collection) return;
    
    pthread_mutex_lock(&collection->mutex);
    
    if (collection->releases) {
        for (int i = 0; i < collection->count; i++) {
            if (collection->releases[i].body) {
                free(collection->releases[i].body);
            }
        }
        free(collection->releases);
    }
    
    pthread_mutex_unlock(&collection->mutex);
    pthread_mutex_destroy(&collection->mutex);
    free(collection);
}

void calculate_time_diff(Release* release) {
    time_t current_time = time(NULL);
    double diff_seconds = difftime(current_time, release->created_at);
    int hours = (int)(diff_seconds / 3600);
    int days = hours / 24;
    
    if (days > 0) {
        snprintf(release->time_difference, MAX_TIME_DIFF_LENGTH, "%dd ago", days);
    } else {
        snprintf(release->time_difference, MAX_TIME_DIFF_LENGTH, "%dh ago", hours);
    }
}

bool add_release_to_collection(ReleaseCollection* collection, Release* release) {
    pthread_mutex_lock(&collection->mutex);
    
    if (collection->count >= collection->capacity) {
        int new_capacity = collection->capacity * 2;
        Release* new_releases = realloc(collection->releases, 
                                      new_capacity * sizeof(Release));
        if (!new_releases) {
            pthread_mutex_unlock(&collection->mutex);
            return false;
        }
        collection->releases = new_releases;
        collection->capacity = new_capacity;
    }
    
    collection->releases[collection->count] = *release;
    collection->count++;
    
    pthread_mutex_unlock(&collection->mutex);
    return true;
}

void fetch_latest_release(RepoInfo* repo, ReleaseCollection* collection, const char* auth_token) {
    CURL* curl;
    CURLcode res;
    MemoryStruct chunk = {0};
    
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Failed to initialize CURL\n");
        free(chunk.memory);
        return;
    }
    
    // Build API URL
    char url[MAX_URL_LENGTH];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/%s/releases/latest", 
             repo->owner, repo->repo);
    
    // Set up headers
    struct curl_slist* headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", auth_token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "User-Agent: nodebro-c/1.0");
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    
    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: CURL failed for %s/%s: %s\n", 
                repo->owner, repo->repo, curl_easy_strerror(res));
    } else {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 200) {
            // Parse JSON response
            cJSON* json = cJSON_Parse(chunk.memory);
            if (json) {
                Release release = {0};
                
                // Copy repo info
                strncpy(release.owner, repo->owner, MAX_REPO_NAME_LENGTH - 1);
                strncpy(release.repo, repo->repo, MAX_REPO_NAME_LENGTH - 1);
                
                // Parse tag name
                cJSON* tag_name = cJSON_GetObjectItemCaseSensitive(json, "tag_name");
                if (cJSON_IsString(tag_name) && tag_name->valuestring) {
                    strncpy(release.tag_name, tag_name->valuestring, MAX_TAG_LENGTH - 1);
                }
                
                // Parse URL
                cJSON* html_url = cJSON_GetObjectItemCaseSensitive(json, "html_url");
                if (cJSON_IsString(html_url) && html_url->valuestring) {
                    strncpy(release.url, html_url->valuestring, MAX_URL_LENGTH - 1);
                }
                
                // Parse body
                cJSON* body = cJSON_GetObjectItemCaseSensitive(json, "body");
                if (cJSON_IsString(body) && body->valuestring) {
                    release.body = strdup(body->valuestring);
                } else {
                    release.body = strdup("No release notes available.");
                }
                
                // Parse prerelease flag
                cJSON* prerelease = cJSON_GetObjectItemCaseSensitive(json, "prerelease");
                if (cJSON_IsBool(prerelease)) {
                    release.prerelease = cJSON_IsTrue(prerelease);
                }
                
                // Parse created_at
                cJSON* created_at = cJSON_GetObjectItemCaseSensitive(json, "created_at");
                if (cJSON_IsString(created_at) && created_at->valuestring) {
                    struct tm tm = {0};
                    // Parse ISO 8601 date
                    sscanf(created_at->valuestring, "%d-%d-%dT%d:%d:%d",
                           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
                    tm.tm_year -= 1900;
                    tm.tm_mon -= 1;
                    release.created_at = mktime(&tm);
                }
                
                calculate_time_diff(&release);
                add_release_to_collection(collection, &release);
                
                cJSON_Delete(json);
            } else {
                fprintf(stderr, "Error: Failed to parse JSON for %s/%s\n", 
                        repo->owner, repo->repo);
            }
        } else {
            fprintf(stderr, "Error: HTTP %ld for %s/%s\n", 
                    response_code, repo->owner, repo->repo);
        }
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

void* fetch_release_thread(void* arg) {
    FetchThreadData* data = (FetchThreadData*)arg;
    fetch_latest_release(&data->repo, data->collection, data->auth_token);
    return NULL;
}

int compare_releases_by_date(const void* a, const void* b) {
    const Release* release_a = (const Release*)a;
    const Release* release_b = (const Release*)b;
    
    // Sort in descending order (newest first)
    if (release_a->created_at > release_b->created_at) return -1;
    if (release_a->created_at < release_b->created_at) return 1;
    return 0;
}

void sort_releases_by_date(ReleaseCollection* collection) {
    pthread_mutex_lock(&collection->mutex);
    
    if (collection->count > 1) {
        qsort(collection->releases, collection->count, sizeof(Release), 
              compare_releases_by_date);
    }
    
    pthread_mutex_unlock(&collection->mutex);
}