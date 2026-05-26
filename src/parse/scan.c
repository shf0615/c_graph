#include "parse/scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

static void add_file(ScanResult *sr, const char *path) {
    if (sr->file_count >= sr->file_capacity) {
        sr->file_capacity *= 2;
        sr->files = realloc(sr->files, sr->file_capacity * sizeof(char *));
    }
    sr->files[sr->file_count++] = strdup(path);
}

static void add_include(ScanResult *sr, const char *dir) {
    for (uint32_t i = 0; i < sr->include_count; i++) {
        if (strcmp(sr->includes[i], dir) == 0) return;
    }
    if (sr->include_count >= sr->include_capacity) {
        sr->include_capacity *= 2;
        sr->includes = realloc(sr->includes, sr->include_capacity * sizeof(char *));
    }
    sr->includes[sr->include_count++] = strdup(dir);
}

static bool has_suffix(const char *name, const char *suffix) {
    size_t nlen = strlen(name);
    size_t slen = strlen(suffix);
    if (nlen < slen) return false;
    return strcmp(name + nlen - slen, suffix) == 0;
}

static void scan_recurse(ScanResult *sr, const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) return;

    bool has_header = false;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_recurse(sr, path);
        } else if (S_ISREG(st.st_mode)) {
            if (has_suffix(ent->d_name, ".c")) {
                add_file(sr, path);
            } else if (has_suffix(ent->d_name, ".h")) {
                has_header = true;
            }
        }
    }
    closedir(d);

    if (has_header) {
        add_include(sr, dir_path);
    }
}

bool scan_directory(ScanResult *sr, const char *root_path) {
    struct stat st;
    if (stat(root_path, &st) != 0 || !S_ISDIR(st.st_mode)) return false;

    memset(sr, 0, sizeof(ScanResult));
    sr->file_capacity = 64;
    sr->files = malloc(sr->file_capacity * sizeof(char *));
    sr->include_capacity = 32;
    sr->includes = malloc(sr->include_capacity * sizeof(char *));

    char resolved[PATH_MAX];
    if (realpath(root_path, resolved)) {
        sr->root_dir = strdup(resolved);
    } else {
        sr->root_dir = strdup(root_path);
    }

    scan_recurse(sr, sr->root_dir);
    return true;
}

void scan_result_destroy(ScanResult *sr) {
    for (uint32_t i = 0; i < sr->file_count; i++) free(sr->files[i]);
    free(sr->files);
    for (uint32_t i = 0; i < sr->include_count; i++) free(sr->includes[i]);
    free(sr->includes);
    free(sr->root_dir);
    memset(sr, 0, sizeof(ScanResult));
}
