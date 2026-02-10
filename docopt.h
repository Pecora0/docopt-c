#ifndef DOCOPT_H
#define DOCOPT_H

typedef enum {
    DOCOPT_PROGRAM_NAME,
    DOCOPT_SUBCOMMAND,
    DOCOPT_ARGUMENT,
    DOCOPT_ELEMENT_COUNT,
} Docopt_Element_Kind;

typedef struct {
    int count;
    Docopt_Element_Kind *kind;
    const char **key;
    const char **value;
} Docopt_Match;

Docopt_Match docopt_interpret(const char *help, int argc, const char **argv);

#endif // DOCOPT_H

#ifdef DOCOPT_IMPLEMENTATION

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define DOCOPT_SHORT_STRLEN 64

typedef struct {
    char it[DOCOPT_SHORT_STRLEN];
} Docopt__Short_String;

Docopt__Short_String docopt__make_short_string(const char *buf, size_t len) {
    assert(len+1 < DOCOPT_SHORT_STRLEN);
    Docopt__Short_String result;
    strncpy(result.it, buf, len);
    result.it[len] = '\0';
    return result;
}

char *docopt__parse_line(char *buf) {
    static char *cursor;
    if (buf != NULL) {
        cursor = buf;
    }

    char *old_cursor = cursor;
    while (1) {
        switch (*cursor) {
            case '\n':
                *cursor = '\0';
                cursor++;
                return old_cursor;
            case '\r':
                assert(0); // TODO: we should handle the EOL of other platforms
            case '\0':
                if (old_cursor == cursor) return NULL;
                return old_cursor;
            default:
                cursor++;
                break;
        }
    }
}

bool docopt__str_isprefix(const char *prefix, const char *str) {
    size_t n = strlen(prefix);
    return 0 == strncmp(str, prefix, n);
}

bool docopt__str_isspace(const char *str) {
    for (const char *c = str; *c != '\0'; c++) {
        if (!isspace(*c)) return false;
    }
    return true;
}

bool docopt__is_argument(const char *str) {
    size_t n = strlen(str);
    if (n == 0) return false;
    if (str[0] == '<' && str[n-1] == '>') return true;
    for (size_t i=0; i<n; i++) {
        if (!isupper(str[i])) return false;
    }
    return true;
}

bool docopt__is_option(const char *str) {
    if (strcmp(str, "-") == 0) return false;
    if (strcmp(str, "--") == 0) return false;
    if (docopt__str_isprefix("-", str)) return true;
    return false;
}

Docopt__Short_String docopt__word(char **rest) {
    Docopt__Short_String result;

    while (isspace(**rest)) (*rest)++;
    const char *str = *rest;
    if (*str == '[' || *str == ']' || *str == '(' || *str == ')' || *str == '|') {
        result.it[0] = str[0];
        result.it[1] = '\0';
        (*rest)++;
        return result;
    }
    if (docopt__str_isprefix("...", str)) {
        strcpy(result.it, "...");
        (*rest) += 3;
        return result;
    }

    size_t n = 0;
    while (1) {
        if (str[n] == '\0') break;
        if (isspace(str[n])) break;
        if (str[n] == '[' || str[n] == ']' || str[n] == '(' || str[n] == ')' || str[n] == '|') break;
        if (docopt__str_isprefix("...", str + n)) break;
        n++;
    }

    assert(n+1 < DOCOPT_SHORT_STRLEN);
    strncpy(result.it, str, n);
    result.it[n] = '\0';

    *rest += n;
    return result;
}

typedef struct {
    Docopt__Short_String key;
    const char *value;
} Docopt__Binding;

typedef struct {
    const char *program_name;
    size_t capacity;
    size_t count;
    Docopt__Binding *items;
} Docopt__Usage_Match;

#define DOCOPT__USAGE_MATCH_INIT_CAP 16

void docopt__append_binding(Docopt__Usage_Match *m, Docopt__Binding b) {
    if (m->capacity == 0) {
        m->capacity = DOCOPT__USAGE_MATCH_INIT_CAP;
        m->count = 0;
        m->items = malloc(m->capacity * sizeof(b));
    }
    if (m->count == m->capacity) {
        m->capacity *= 2;
        m->items = realloc(m->items, m->capacity * sizeof(b));
    }
    assert(m->count < m->capacity);
    m->items[m->count] = b;
    m->count++;
}

typedef enum {
    DOCOPT__UPATTERN_ROOT,
    DOCOPT__UPATTERN_SIMPLE,
    DOCOPT__UPATTERN_GROUP,

    DOCOPT__UPATTERN_KIND_COUNT,
} Docopt__UPattern_Kind;

typedef struct Docopt__UPattern {
    Docopt__UPattern_Kind kind;
    Docopt__Short_String name;
    bool optional;
    bool alternative;
    bool repeat;
    size_t capacity;
    size_t count;
    struct Docopt__UPattern *child;
} Docopt__UPattern;

void docopt__append_upattern(Docopt__UPattern *lst, Docopt__UPattern p) {
    if (lst->capacity == 0) {
        lst->capacity = 16;
        lst->count = 0;
        lst->child = malloc(lst->capacity * sizeof(p));
    }
    if (lst->count == lst->capacity) {
        lst->capacity *= 2;
        lst->child = realloc(lst->child, lst->capacity * sizeof(p));
        assert(lst->child != NULL);
    }
    assert(lst->count < lst->capacity);
    lst->child[lst->count] = p;
    lst->count++;
}

bool docopt__compile_upattern_ex(char **code, Docopt__UPattern *result) {
    Docopt__Short_String word = docopt__word(code);
    if (word.it[0] == '\0') return false;
    if (docopt__is_argument(word.it)) {
        Docopt__UPattern child = {
            .kind = DOCOPT__UPATTERN_SIMPLE,
            .optional = false,
            .name = word,
        };
        docopt__append_upattern(result, child);
    } else if (docopt__is_option(word.it)) {
        Docopt__UPattern child = {
            .kind = DOCOPT__UPATTERN_SIMPLE,
            .optional = false,
            .name = word,
        };
        docopt__append_upattern(result, child);
    } else if (strcmp("[", word.it) == 0) {
        Docopt__UPattern child = {
            .kind = DOCOPT__UPATTERN_GROUP,
            .optional = true,
            .count = 0,
        };
        while (docopt__compile_upattern_ex(code, &child)) { }
        docopt__append_upattern(result, child);
    } else if (strcmp("]", word.it) == 0) {
        return false;
    } else if (strcmp("(", word.it) == 0) {
        Docopt__UPattern child = {
            .kind = DOCOPT__UPATTERN_GROUP,
            .optional = false,
            .alternative = false,
            .count = 0,
        };
        while (docopt__compile_upattern_ex(code, &child)) { }
        docopt__append_upattern(result, child);
    } else if (strcmp(")", word.it) == 0) {
        return false;
    } else if (strcmp("|", word.it) == 0) {
        assert(result->kind == DOCOPT__UPATTERN_GROUP);
        if (!result->alternative) {
            assert(result->count > 0);
            if (result->count > 1) {
                assert(0);
            }
            result->alternative = true;
        }
    } else if (strcmp("...", word.it) == 0) {
        assert(result->count > 0);
        Docopt__UPattern before = result->child[result->count-1];
        Docopt__UPattern child = {
            .kind = DOCOPT__UPATTERN_GROUP,
            .optional = before.optional,
            .repeat = true,
            .count = 0,
        };
        docopt__append_upattern(&child, before);
        result->child[result->count-1] = child;
    } else {
        Docopt__UPattern child = {
            .kind = DOCOPT__UPATTERN_SIMPLE,
            .optional = false,
            .name = word,
        };
        docopt__append_upattern(result, child);
    }
    return true;
}

Docopt__UPattern docopt__compile_upattern(const char *code) {
    Docopt__UPattern result = {0};

    result.kind = DOCOPT__UPATTERN_ROOT;
    char *code_cpy = strdup(code); // TODO: free this memory
    result.name = docopt__word(&code_cpy);
    assert(result.name.it[0] != '\0');

    while (docopt__compile_upattern_ex(&code_cpy, &result)) {}

    return result;
}

bool docopt__match_upattern(Docopt__UPattern pattern, const char **argv, int argc, Docopt__Usage_Match *result) {
    switch (pattern.kind) {
        case DOCOPT__UPATTERN_ROOT:
            Docopt__Short_String name = pattern.name;
            assert(strlen(name.it) > 0);
            assert(argc > 0);
            if (strcmp(name.it, argv[0]) != 0) {
                return false;
            }
            result->program_name = argv[0];
            argv++; argc--;

            for (size_t i=0; i<pattern.count; i++) {
                if (!docopt__match_upattern(pattern.child[i], argv, argc, result)) return false;
            }
            return true;
        case DOCOPT__UPATTERN_SIMPLE:
            assert(argc > 0);
            if (docopt__is_option(pattern.name.it)) assert(0);
            if (docopt__is_argument(pattern.name.it)) assert(0);
            if (strcmp(pattern.name.it, argv[0]) != 0) return false;
            argv++; argc--;
            return true;
        case DOCOPT__UPATTERN_GROUP:
            assert(0);
        case DOCOPT__UPATTERN_KIND_COUNT:
            assert(0);
    }
    assert(0);
}

#define DOCOPT__OPTION_KEY_CAPACITY 4

typedef struct {
    Docopt__Short_String key[DOCOPT__OPTION_KEY_CAPACITY];
    Docopt__Short_String value;
    const char *def;
} Docopt__OPattern;

Docopt__Short_String docopt__oword(char **code) {
    Docopt__Short_String result;
    result.it[0] = '\0';
    char *cursor = *code;
    if (cursor[0] == '\0') return result;
    if (docopt__str_isprefix("  ", cursor)) return result;

    while (isspace(cursor[0]) || cursor[0] == ',' || cursor[0] == '=') cursor++;
    size_t len = 0;
    while (cursor[len] != '\0' && !isspace(cursor[len]) && cursor[len] != ',' && cursor[len] != '=') {
        result.it[len] = cursor[len];
        len++;
        assert(len < DOCOPT_SHORT_STRLEN);
    }

    result.it[len] = '\0';
    cursor += len;
    *code = cursor;
    return result;
}

Docopt__OPattern docopt__compile_opattern(const char *code) {
    while (isspace(code[0])) code++;
    assert(code[0] == '-');
    char *code_cpy = strdup(code); // TODO: free this memory

    Docopt__OPattern result = {0};
    size_t key_count = 0;
    for (
            Docopt__Short_String word = docopt__oword(&code_cpy); 
            word.it[0] != '\0'; 
            word = docopt__oword(&code_cpy)
            ) {
        if (word.it[0] == '-') {
            assert(key_count < DOCOPT__OPTION_KEY_CAPACITY);
            result.key[key_count] = word;
            key_count++;
        } else {
            result.value = word;
        }
    }
    // TODO: handle default values
    result.def = NULL;
    return result;
}

typedef struct {
    size_t upattern_count;
    Docopt__UPattern *upattern;
    size_t opattern_count;
    Docopt__OPattern *opattern;
} Docopt__Pattern;

Docopt__Pattern docopt__compile_pattern(const char *msg) {
    Docopt__Pattern result = {0};
    size_t upattern_cap = 16;
    size_t opattern_cap = 16;
    result.upattern = malloc(upattern_cap * sizeof(Docopt__UPattern));
    result.opattern = malloc(opattern_cap * sizeof(Docopt__OPattern));

    char *msg_cpy = strdup(msg); // TODO: free this memory

    enum {
        STATE_START,
        STATE_USAGE,
        STATE_OPTIONS,
    } state = STATE_START;

    for (const char *line = docopt__parse_line(msg_cpy); line; line = docopt__parse_line(NULL)) {
        switch (state) {
            case STATE_START:
                if (docopt__str_isprefix("Usage:", line)) {
                    if (!docopt__str_isspace(line + strlen("Usage:"))) {
                        // TODO: handle the usage pattern that is on this line
                        assert(0);
                    }
                    state = STATE_USAGE;
                }
                if (docopt__str_isprefix("Options:", line)) {
                    state = STATE_OPTIONS;
                }
                break;
            case STATE_USAGE:
                if (docopt__str_isspace(line)) {
                    state = STATE_START;
                    break;
                }
                {
                    Docopt__UPattern p = docopt__compile_upattern(line);
                    // TODO: reallocate if capacity is exceeded
                    assert(result.upattern_count < upattern_cap);
                    result.upattern[result.upattern_count] = p;
                    result.upattern_count++;
                }
                break;
            case STATE_OPTIONS:
                if (docopt__str_isspace(line)) {
                    state = STATE_START;
                    break;
                }
                {
                    Docopt__OPattern p = docopt__compile_opattern(line);
                    // TODO: reallocate if capacity is exceeded
                    assert(result.opattern_count < opattern_cap);
                    result.opattern[result.opattern_count] = p;
                    result.opattern_count++;
                }
                break;
        }
    }
    return result;
}

bool docopt__umatch(Docopt__UPattern p, int argc, const char **argv, Docopt_Match *m) {
    switch (p.kind) {
        case DOCOPT__UPATTERN_ROOT:
            if (argc == 0) return false;
            assert(argc > 0);
            m->count = 0;
            m->kind[0]  = DOCOPT_PROGRAM_NAME;
            m->key[0]   = strdup(p.name.it);
            m->value[0] = argv[0];
            m->count++;
            for (size_t i=0; i<p.count; i++) {
                assert(0);
            }
            return true;
        case DOCOPT__UPATTERN_SIMPLE:
            assert(0);
        case DOCOPT__UPATTERN_GROUP:
            assert(0);
        case DOCOPT__UPATTERN_KIND_COUNT:
            assert(0);
    }
    assert(0);
}

Docopt_Match docopt__match(Docopt__Pattern p, int argc, const char **argv) {
    Docopt_Match m;
    assert(argc > 0);
    m.kind  = calloc(argc, sizeof(m.kind[0]));
    m.key   = calloc(argc, sizeof(m.key[0]));
    m.value = calloc(argc, sizeof(m.value[0]));
    for (size_t i=0; i<p.upattern_count; i++) {
        if (docopt__umatch(p.upattern[i], argc, argv, &m)) break;
    }
    assert(0);
}

Docopt_Match docopt_interpret(const char *help, int argc, const char **argv) {
    Docopt__Pattern p = docopt__compile_pattern(help);
    return docopt__match(p, argc, argv);
    //TODO: free the memory allocated by docopt__compile_pattern
}

#endif // DOCOPT_IMPLEMENTATION

#ifdef DOCOPT_UTILITY

#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char **argv) {
    (void) argc;
    (void) argv;

    fprintf(stderr, "[TODO]: utility not implemented\n");
    abort();
}

#endif //DOCOPT_UTILITY
