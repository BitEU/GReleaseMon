#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

// Error codes
typedef enum {
    SUCCESS = 0,
    ERROR_CONFIG_NOT_FOUND,
    ERROR_CONFIG_INVALID,
    ERROR_NETWORK_FAILURE,
    ERROR_JSON_PARSE,
    ERROR_OUT_OF_MEMORY,
    ERROR_HTTP_INIT,
    ERROR_UI_INIT
} ErrorCode;

// Function declarations
void handle_error(ErrorCode error, const char* context);
const char* get_error_message(ErrorCode error);
void log_message(const char* format, ...);
bool file_exists(const char* path);
void msleep(int milliseconds);

#endif // UTILS_H