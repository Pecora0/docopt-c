#ifndef DOCOPT_H
#define DOCOPT_H

typedef enum {
    DOCOPT_PROGRAM_NAME,
    DOCOPT_SUBCOMMAND,
    DOCOPT_ARGUMENT,
    DOCOPT_OPTION,
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
    struct Docopt__UPattern *head;
    struct Docopt__UPattern *rest;
} Docopt__UPattern;

static Docopt__UPattern *docopt__new_upattern_simple(const char *name) {
    // TODO: allocate the struct in an arena or something similar such that it can be freed all at once
    Docopt__UPattern *result = malloc(sizeof(Docopt__UPattern));
    assert(result != NULL);
    result->kind = DOCOPT__UPATTERN_SIMPLE;
    result->name = docopt__make_short_string(name, strlen(name));
    return result;
}

static Docopt__UPattern *docopt__new_upattern_group() {
    Docopt__UPattern *result = malloc(sizeof(Docopt__UPattern));
    assert(result != NULL);
    memset(result, 0, sizeof(Docopt__UPattern));
    result->kind = DOCOPT__UPATTERN_GROUP;
    return result;
}

void docopt__compile_upattern_ex(char **code, Docopt__UPattern *result) {
    Docopt__Short_String word = docopt__word(code);
    if (word.it[0] == '\0') {
        assert(result->head != NULL);
        return;
    }

    Docopt__UPattern *head;
    if (docopt__is_argument(word.it)) {
        head = docopt__new_upattern_simple(word.it);
    } else if (docopt__is_option(word.it)) {
        head = docopt__new_upattern_simple(word.it);
    } else if (strcmp("[", word.it) == 0) {
        head = docopt__new_upattern_group();
        head->optional = true;
        docopt__compile_upattern_ex(code, head);
    } else if (strcmp("]", word.it) == 0) {
        assert(result->head != NULL);
        return;
    } else if (strcmp("(", word.it) == 0) {
        head = docopt__new_upattern_group();
        docopt__compile_upattern_ex(code, head);
    } else if (strcmp(")", word.it) == 0) {
        assert(result->head != NULL);
        return;
    } else if (strcmp("|", word.it) == 0) {
        assert(result->kind == DOCOPT__UPATTERN_GROUP);
        assert(result->head != NULL);
        result->alternative = true;
        docopt__compile_upattern_ex(code, result);
        return;
    } else if (strcmp("...", word.it) == 0) {
        assert(result->kind == DOCOPT__UPATTERN_GROUP);
        assert(result->head != NULL);
        result->repeat = true;
        return;
    } else {
        head = docopt__new_upattern_simple(word.it);
    }

    if (result->head == NULL) {
        result->head = head;
        result->rest = NULL;

        docopt__compile_upattern_ex(code, result);

        return;
    } else {
        assert(result->rest == NULL);

        result->rest = docopt__new_upattern_group();
        result->rest->head = head;
        docopt__compile_upattern_ex(code, result->rest);

        return;
    }
}

Docopt__UPattern docopt__compile_upattern(const char *code) {
    Docopt__UPattern result = {0};

    result.kind = DOCOPT__UPATTERN_ROOT;
    char *code_cpy = strdup(code); // TODO: free this memory

    docopt__compile_upattern_ex(&code_cpy, &result);

    return result;
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

void docopt__append_match(Docopt_Match *m, Docopt_Element_Kind kind, const char *key, const char *val) {
    size_t n = m->count;
    m->kind[n] = kind;
    m->key[n] = key;
    m->value[n] = val;
    m->count++;
}

int docopt__umatch(Docopt__UPattern p, int argc, const char **argv, Docopt_Match *m) {
    switch (p.kind) {
        case DOCOPT__UPATTERN_ROOT:
            {
                if (argc == 0) return 0;
                assert(argc > 0);
                m->count = 0;

                assert(p.head->kind == DOCOPT__UPATTERN_SIMPLE);
                docopt__append_match(m, DOCOPT_PROGRAM_NAME, strdup(p.head->name.it), argv[0]);
                int n1 = 1;

                int n2 = docopt__umatch(*p.rest, argc-1, argv+1, m);
                return n1+n2;
            }
        case DOCOPT__UPATTERN_SIMPLE:
            if (argc == 0) return 0;
            assert(argc > 0);
            if (docopt__is_option(p.name.it)) {
                assert(0);
            } else if (docopt__is_argument(p.name.it)) {
                docopt__append_match(m, DOCOPT_ARGUMENT, strdup(p.name.it), argv[0]);
                return 1;
            } else {
                if (strcmp(p.name.it, argv[0]) == 0) {
                    docopt__append_match(m, DOCOPT_SUBCOMMAND, NULL, argv[0]);
                    return 1;
                }
                return 0;
            }
            assert(0);
        case DOCOPT__UPATTERN_GROUP:
            {
                assert(p.head != NULL);
                if (p.optional) assert(0);
                if (p.alternative) assert(0);
                if (p.repeat) {
                    if (p.rest == NULL) {
                        int total = 0;
                        int n = docopt__umatch(*p.head, argc, argv, m);
                        if (n == 0) return 0;
                        assert(n > 0);
                        total += n;
                        while (total <= argc && n > 0) {
                            n = docopt__umatch(*p.head, argc-total, argv+total, m);
                            assert(n >= 0);
                            total += n;
                        }
                        return total;
                    }
                    assert(0);
                }
                int n1 = docopt__umatch(*p.head, argc, argv, m);
                assert(n1 > 0);
                assert(n1 <= argc);
                if (p.rest != NULL) {
                    int n2 = docopt__umatch(*p.rest, argc-n1, argv+n1, m);
                    return n1 + n2;
                }
                return n1;
            }
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
        if (docopt__umatch(p.upattern[i], argc, argv, &m) == argc) break;
    }
    for (int i=0; i<m.count; i++) {
        if (m.kind[i] == DOCOPT_OPTION) {
            assert(0);
        }
    }
    return m;
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
