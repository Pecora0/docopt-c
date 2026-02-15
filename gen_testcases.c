#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>

#define FILE_PATH "testcases.docopt"

#define QSTART "r\"\"\""
#define QEND   "\"\"\""

typedef enum {
    START,
    HELP_MSG,
    EXPECT,
} State;

bool is_prefix(const char *prefix, const char *str) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool is_suffix(const char *suffix, const char *str) {
    size_t n = strlen(suffix);
    if (n > strlen(str)) return false;
    return strcmp(suffix, str + strlen(str) - n) == 0;
}

void strip_right(char *str) {
    for (size_t i=0; i<strlen(str); i++) {
        if (str[i] == '#') {
            str[i] = '\0';
            return;
        }
    }
    for (size_t i=strlen(str); i>0 && isspace(str[i-1]); i--) {
        str[i-1] = '\0';
    }
}

int main() {
    FILE *file = fopen(FILE_PATH, "r");
    if (file == NULL) {
        fprintf(stderr, "[ERROR] Can not open file %s: %s\n", FILE_PATH, strerror(errno));
        exit(1);
    }

    size_t line_nr = 1;
    char *line = NULL;
    size_t n = 0;
    State state = START;
    for (ssize_t r = getline(&line, &n, file); r >= 0; r = getline(&line, &n, file)) {
        strip_right(line);
        if (is_prefix(QSTART, line)) {
            assert(state == START);
            state = HELP_MSG;
            if (is_suffix(QEND, line)) {
                line[strlen(line)-strlen(QEND)] = '\0';
                state = START;
            }
            printf("HELP_MSG: %s\n", line + strlen("r\"\"\""));
        } else if (is_prefix("$ ", line)) {
            assert(state == START);
            printf("PROMPT: %s\n", line + strlen("$ "));
        } else if (is_prefix("{", line)) {
            assert(state == START);
            printf("EXPECT: %s\n", line);
            state = EXPECT;
        } else if (is_prefix("\"user-error\"", line)) {
            assert(state == START);
            printf("EXPECT-ERROR\n");
        } else {
            switch (state) {
                case START:
                    assert(line[0] == '\0');
                    break;
                case HELP_MSG:
                    if (is_suffix(QEND, line)) {
                        line[strlen(line)-strlen(QEND)] = '\0';
                        state = START;
                    }
                    printf("HELP_MSG: %s\n", line);
                    break;
                case EXPECT:
                    if (line[0] == '\0') {
                        state = START;
                        break;
                    }
                    printf("EXPECT: %s\n", line);
                    break;
            }
        }
        line_nr++;
    }
    free(line);

    fclose(file);
}
