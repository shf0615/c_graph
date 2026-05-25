#include "parse/compdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static const char *parse_string(const char *p, char **out) {
    if (*p != '"') return NULL;
    p++;
    /* Handle escape sequences */
    size_t cap = 128;
    char *buf = malloc(cap);
    size_t len = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p+1)) {
            p++;
        }
        if (len >= cap - 1) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = *p++;
    }
    buf[len] = '\0';
    *out = buf;
    if (*p == '"') p++;
    return p;
}

static void parse_command_to_args(CompDBEntry *entry) {
    char *cmd = strdup(entry->command);
    uint32_t cap = 16;
    entry->args = malloc(cap * sizeof(char *));
    entry->argc = 0;
    char *tok = strtok(cmd, " ");
    while (tok) {
        if (entry->argc >= cap) {
            cap *= 2;
            entry->args = realloc(entry->args, cap * sizeof(char *));
        }
        entry->args[entry->argc++] = strdup(tok);
        tok = strtok(NULL, " ");
    }
    entry->args = realloc(entry->args, (entry->argc + 1) * sizeof(char *));
    entry->args[entry->argc] = NULL;
    free(cmd);
}

bool compdb_parse(CompDB *db, const char *path) {
    size_t len;
    char *json = read_file(path, &len);
    if (!json) return false;

    db->count = 0;
    db->capacity = 32;
    db->entries = calloc(db->capacity, sizeof(CompDBEntry));

    const char *p = skip_ws(json);
    if (*p != '[') { free(json); return false; }
    p = skip_ws(p + 1);

    while (*p && *p != ']') {
        if (*p == ',') { p = skip_ws(p + 1); continue; }
        if (*p != '{') break;
        p = skip_ws(p + 1);

        if (db->count >= db->capacity) {
            db->capacity *= 2;
            db->entries = realloc(db->entries, db->capacity * sizeof(CompDBEntry));
        }
        CompDBEntry *e = &db->entries[db->count];
        memset(e, 0, sizeof(CompDBEntry));

        while (*p && *p != '}') {
            if (*p == ',') { p = skip_ws(p + 1); continue; }
            char *key = NULL;
            p = parse_string(p, &key);
            if (!p) break;
            p = skip_ws(p);
            if (*p == ':') p = skip_ws(p + 1);

            if (*p == '"') {
                char *val = NULL;
                p = parse_string(p, &val);
                if (!p) { free(key); break; }
                p = skip_ws(p);

                if (strcmp(key, "directory") == 0) e->directory = val;
                else if (strcmp(key, "file") == 0) e->file = val;
                else if (strcmp(key, "command") == 0) e->command = val;
                else free(val);
            } else if (*p == '[') {
                /* "arguments": [...] - skip for now, use command */
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[') depth++;
                    else if (*p == ']') depth--;
                    p++;
                }
                p = skip_ws(p);
            }
            free(key);
        }
        if (*p == '}') p = skip_ws(p + 1);

        if (e->command) parse_command_to_args(e);
        db->count++;
    }

    free(json);
    return true;
}

void compdb_destroy(CompDB *db) {
    for (uint32_t i = 0; i < db->count; i++) {
        free(db->entries[i].directory);
        free(db->entries[i].file);
        free(db->entries[i].command);
        if (db->entries[i].args) {
            for (uint32_t j = 0; j < db->entries[i].argc; j++)
                free(db->entries[i].args[j]);
            free(db->entries[i].args);
        }
    }
    free(db->entries);
}
