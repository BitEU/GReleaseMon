#include "requests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// Simple JSON string extraction function
char* extract_json_string(const char* json, const char* key) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* start = strstr(json, search_key);
    if (!start) return NULL;
    
    start += strlen(search_key);
    
    // Skip whitespace
    while (*start == ' ' || *start == '\t' || *start == '\n') start++;
    
    if (*start != '"') return NULL;
    start++; // Skip opening quote
    
    char* end = start;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end + 1)) {
            end += 2; // Skip escaped character
        } else {
            end++;
        }
    }
    
    if (*end != '"') return NULL;
    
    size_t len = end - start;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, start, len);
    result[len] = '\0';
    
    return result;
}

// Simple JSON boolean extraction function
bool extract_json_bool(const char* json, const char* key) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* start = strstr(json, search_key);
    if (!start) return false;
    
    start += strlen(search_key);
    
    // Skip whitespace
    while (*start == ' ' || *start == '\t' || *start == '\n') start++;
    
    return (strncmp(start, "true", 4) == 0);
}

// Check if asset name contains Windows-related keywords
bool is_windows_asset(const char* name) {
    if (!name) return false;
    
    // Convert to lowercase for case-insensitive comparison
    char lower_name[512];
    int len = strlen(name);
    if (len >= sizeof(lower_name)) len = sizeof(lower_name) - 1;
    
    for (int i = 0; i < len; i++) {
        lower_name[i] = tolower(name[i]);
    }
    lower_name[len] = '\0';
    
    // Check for .exe extension
    if (strstr(lower_name, ".exe")) return true;
    if (strstr(lower_name, ".msi")) return true;
    
    // Check for Windows-related keywords
    if (strstr(lower_name, "windows")) return true;
    if (strstr(lower_name, "win")) return true;
    if (strstr(lower_name, "msvc")) return true;
    if (strstr(lower_name, "window")) return true;
    
    return false;
}

// Check if release has Windows assets
bool check_windows_assets(const char* json) {
    // Look for assets array
    char* assets_start = strstr(json, "\"assets\":");
    if (!assets_start) return false;
    
    // Find the opening bracket of the array
    char* bracket = strchr(assets_start, '[');
    if (!bracket) return false;
    
    // Find the closing bracket
    char* end_bracket = bracket;
    int bracket_count = 0;
    while (*end_bracket) {
        if (*end_bracket == '[') bracket_count++;
        else if (*end_bracket == ']') {
            bracket_count--;
            if (bracket_count == 0) break;
        }
        end_bracket++;
    }
    
    if (bracket_count != 0) return false;
    
    // Extract assets section
    size_t assets_len = end_bracket - bracket + 1;
    char* assets_json = malloc(assets_len + 1);
    if (!assets_json) return false;
    
    strncpy(assets_json, bracket, assets_len);
    assets_json[assets_len] = '\0';
    
    // Look for asset names
    char* name_pos = assets_json;
    while ((name_pos = strstr(name_pos, "\"name\":")) != NULL) {
        name_pos += 7; // Skip "name":
        
        // Skip whitespace
        while (*name_pos == ' ' || *name_pos == '\t' || *name_pos == '\n') name_pos++;
        
        if (*name_pos == '"') {
            name_pos++; // Skip opening quote
            
            // Find closing quote
            char* name_end = name_pos;
            while (*name_end && *name_end != '"') {
                if (*name_end == '\\' && *(name_end + 1)) {
                    name_end += 2; // Skip escaped character
                } else {
                    name_end++;
                }
            }
            
            if (*name_end == '"') {
                // Extract name and check
                size_t name_len = name_end - name_pos;
                char asset_name[512];
                if (name_len < sizeof(asset_name)) {
                    strncpy(asset_name, name_pos, name_len);
                    asset_name[name_len] = '\0';
                    
                    if (is_windows_asset(asset_name)) {
                        free(assets_json);
                        return true;
                    }
                }
            }
            name_pos = name_end;
        }
    }
    
    free(assets_json);
    return false;
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
    InitializeCriticalSection(&collection->mutex);
    
    return collection;
}

void free_release_collection(ReleaseCollection* collection) {
    if (!collection) return;
    
    EnterCriticalSection(&collection->mutex);
    
    if (collection->releases) {
        for (int i = 0; i < collection->count; i++) {
            if (collection->releases[i].body) {
                free(collection->releases[i].body);
            }
        }
        free(collection->releases);
    }
    
    LeaveCriticalSection(&collection->mutex);
    DeleteCriticalSection(&collection->mutex);
    free(collection);
}

void calculate_time_diff(Release* release) {
    time_t current_time = time(NULL);
    double diff_seconds = difftime(current_time, release->created_at);
    int seconds = (int)diff_seconds;
    int minutes = seconds / 60;
    int hours = minutes / 60;
    int days = hours / 24;
    int years = days / 365;
    int months = (days % 365) / 30;
    int rem_days = (days % 365) % 30;
    char buf[MAX_TIME_DIFF_LENGTH] = "";

    if (years > 0) {
        snprintf(buf, sizeof(buf), "%dy", years);
    }
    if (months > 0) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "%dmo", months);
    }
    if (rem_days > 0 && years == 0) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "%dd", rem_days);
    }
    if (days >= 1 && years == 0 && months == 0) {
        snprintf(release->time_difference, MAX_TIME_DIFF_LENGTH, "%dd ago", days);
        return;
    }
    if (years > 0 || months > 0) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, " ago");
        strncpy(release->time_difference, buf, MAX_TIME_DIFF_LENGTH);
        return;
    }
    if (hours > 0) {
        snprintf(release->time_difference, MAX_TIME_DIFF_LENGTH, "%dh ago", hours);
    } else if (minutes > 0) {
        snprintf(release->time_difference, MAX_TIME_DIFF_LENGTH, "%dmin ago", minutes);
    } else {
        snprintf(release->time_difference, MAX_TIME_DIFF_LENGTH, "%ds ago", seconds);
    }
}

bool add_release_to_collection(ReleaseCollection* collection, Release* release) {
    EnterCriticalSection(&collection->mutex);
    
    if (collection->count >= collection->capacity) {
        int new_capacity = collection->capacity * 2;
        Release* new_releases = realloc(collection->releases, 
                                      new_capacity * sizeof(Release));
        if (!new_releases) {
            LeaveCriticalSection(&collection->mutex);
            return false;
        }
        collection->releases = new_releases;
        collection->capacity = new_capacity;
    }
    
    collection->releases[collection->count] = *release;
    collection->count++;
    
    LeaveCriticalSection(&collection->mutex);
    return true;
}

void fetch_latest_release(RepoInfo* repo, ReleaseCollection* collection, const char* auth_token) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    DWORD dwStatusCode = 0;
    DWORD dwStatusCodeSize = sizeof(dwStatusCode);
    
    char* pszOutBuffer = NULL;
    BOOL bResults = FALSE;
    
    // Initialize WinHTTP
    hSession = WinHttpOpen(L"GReleaseMon-c/1.0", 
                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS,
                          0);
    
    if (!hSession) {
        fprintf(stderr, "Error: Failed to initialize WinHTTP\n");
        return;
    }
    
    // Connect to GitHub API
    hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    
    if (!hConnect) {
        fprintf(stderr, "Error: Failed to connect to GitHub API\n");
        goto cleanup;
    }
    
    // Build request path
    wchar_t wszPath[512];
    swprintf(wszPath, sizeof(wszPath)/sizeof(wchar_t), 
             L"/repos/%hs/%hs/releases/latest", repo->owner, repo->repo);
    
    // Create HTTP request
    hRequest = WinHttpOpenRequest(hConnect, L"GET", wszPath, NULL, 
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                 WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        fprintf(stderr, "Error: Failed to create HTTP request\n");
        goto cleanup;
    }
    
    // Add headers
    wchar_t wszHeaders[1024];
    swprintf(wszHeaders, sizeof(wszHeaders)/sizeof(wchar_t), 
             L"Authorization: Bearer %hs\r\n"
             L"User-Agent: GReleaseMon-c/1.0\r\n"
             L"Accept: application/vnd.github.v3+json\r\n",
             auth_token);
    
    // Send request
    bResults = WinHttpSendRequest(hRequest, wszHeaders, -1, 
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    
    if (!bResults) {
        fprintf(stderr, "Error: Failed to send HTTP request for %s/%s\n", 
                repo->owner, repo->repo);
        goto cleanup;
    }
    
    // Receive response
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    
    if (!bResults) {
        fprintf(stderr, "Error: Failed to receive HTTP response for %s/%s\n", 
                repo->owner, repo->repo);
        goto cleanup;
    }
    
    // Check status code
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwStatusCodeSize, 
                       WINHTTP_NO_HEADER_INDEX);
    
    if (dwStatusCode == 404) { // Not Found
        // Repo has no releases, create a placeholder
        Release release = {0};
        strncpy(release.owner, repo->owner, MAX_REPO_NAME_LENGTH - 1);
        strncpy(release.repo, repo->repo, MAX_REPO_NAME_LENGTH - 1);
        strncpy(release.tag_name, "None", MAX_TAG_LENGTH - 1);
        // Leave other fields blank
        
        add_release_to_collection(collection, &release);
        goto cleanup;
        
    } else if (dwStatusCode != 200) {
        fprintf(stderr, "Error: HTTP %lu for %s/%s\n", 
                dwStatusCode, repo->owner, repo->repo);
        goto cleanup;
    }
    
    // Read response data
    char* response_data = NULL;
    DWORD total_size = 0;
    
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            fprintf(stderr, "Error: Failed to query data available\n");
            goto cleanup;
        }
        
        if (dwSize == 0) break;
        
        response_data = realloc(response_data, total_size + dwSize + 1);
        if (!response_data) {
            fprintf(stderr, "Error: Out of memory\n");
            goto cleanup;
        }
        
        if (!WinHttpReadData(hRequest, response_data + total_size, dwSize, &dwDownloaded)) {
            fprintf(stderr, "Error: Failed to read data\n");
            free(response_data);
            goto cleanup;
        }
        
        total_size += dwDownloaded;
    } while (dwSize > 0);
    
    if (response_data) {
        response_data[total_size] = '\0';
        
        // Parse JSON response using simple string functions
        Release release = {0};
        
        // Copy repo info
        strncpy(release.owner, repo->owner, MAX_REPO_NAME_LENGTH - 1);
        strncpy(release.repo, repo->repo, MAX_REPO_NAME_LENGTH - 1);
        
        // Parse tag name
        char* tag_name = extract_json_string(response_data, "tag_name");
        if (tag_name) {
            strncpy(release.tag_name, tag_name, MAX_TAG_LENGTH - 1);
            free(tag_name);
        }
        
        // Parse URL
        char* html_url = extract_json_string(response_data, "html_url");
        if (html_url) {
            strncpy(release.url, html_url, MAX_URL_LENGTH - 1);
            free(html_url);
        }
        
        // Parse body
        char* body = extract_json_string(response_data, "body");
        if (body) {
            release.body = body;
        } else {
            release.body = strdup("No release notes available.");
        }
        
        // Parse prerelease flag
        release.prerelease = extract_json_bool(response_data, "prerelease");
        
        // Check for Windows assets
        release.has_windows_assets = check_windows_assets(response_data);
        
        // Parse created_at
        char* created_at = extract_json_string(response_data, "created_at");
        if (created_at) {
            struct tm tm = {0};
            // Parse ISO 8601 date
            sscanf(created_at, "%d-%d-%dT%d:%d:%d",
                   &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                   &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
            tm.tm_year -= 1900;
            tm.tm_mon -= 1;
            // Use timegm to treat parsed time as UTC, not local time
            #ifdef _WIN32
            // Windows does not have timegm, so use _mkgmtime
            release.created_at = _mkgmtime(&tm);
            #else
            release.created_at = timegm(&tm);
            #endif
            free(created_at);
        }
        
        calculate_time_diff(&release);
        add_release_to_collection(collection, &release);
        
        free(response_data);
    }
    
cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
}

unsigned __stdcall fetch_release_thread(void* arg) {
    FetchThreadData* data = (FetchThreadData*)arg;
    fetch_latest_release(&data->repo, data->collection, data->auth_token);
    return 0;
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
    EnterCriticalSection(&collection->mutex);
    
    if (collection->count > 1) {
        qsort(collection->releases, collection->count, sizeof(Release), 
              compare_releases_by_date);
    }
    
    LeaveCriticalSection(&collection->mutex);
}