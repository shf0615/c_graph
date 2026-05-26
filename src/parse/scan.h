#ifndef CGRAPH_SCAN_H
#define CGRAPH_SCAN_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char **files;         /* absolute paths to .c files */
    uint32_t file_count;
    uint32_t file_capacity;
    char **includes;      /* -I directories (dirs containing .h files) */
    uint32_t include_count;
    uint32_t include_capacity;
    char *root_dir;       /* canonical root path */
} ScanResult;

bool scan_directory(ScanResult *sr, const char *root_path);
void scan_result_destroy(ScanResult *sr);

#endif
