#include "utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <Windows.h>

const char* get_error_message(ErrorCode error) {
    switch (error) {
        case SUCCESS:
            return "Success";
        case ERROR_CONFIG_NOT_FOUND:
            return "Configuration file not found";
        case ERROR_CONFIG_INVALID:
            return "Invalid configuration file";
        case ERROR_NETWORK_FAILURE:
            return "Network request failed";
        case ERROR_JSON_PARSE:
            return "Failed to parse JSON response";
        case ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case ERROR_HTTP_INIT:
            return "Failed to initialize HTTP";
        case ERROR_UI_INIT:
            return "Failed to initialize UI";
        default:
            return "Unknown error";
    }
}

void handle_error(ErrorCode error, const char* context) {
    fprintf(stderr, "Error: %s", get_error_message(error));
    if (context) {
        fprintf(stderr, " - %s", context);
    }
    fprintf(stderr, "\n");
}

void log_message(const char* format, ...) {
    time_t now;
    time(&now);
    struct tm* local = localtime(&now);
    
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local);
    
    fprintf(stderr, "[%s] ", timestamp);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

bool file_exists(const char* path) {
    DWORD attributes = GetFileAttributes(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && 
            !(attributes & FILE_ATTRIBUTE_DIRECTORY));
}

void msleep(int milliseconds) {
    Sleep(milliseconds);
}