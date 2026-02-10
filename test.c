#define DOCOPT_IMPLEMENTATION
#include "docopt.h"

#include "munit/munit.h"
#include <stdio.h>

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

static bool upattern_equal(Docopt__UPattern p, Docopt__UPattern q) {
    munit_assert_int(p.kind, ==, q.kind);
    munit_assert(p.optional == q.optional);
    switch (p.kind) {
        case DOCOPT__UPATTERN_ROOT:
            munit_assert_string_equal(p.name.it, q.name.it);
            munit_assert_size(p.count, ==, q.count);
            for (size_t i=0; i<p.count; i++) {
                if (!upattern_equal(p.child[i], q.child[i])) return false;
            }
            break;
        case DOCOPT__UPATTERN_COMMAND:
        case DOCOPT__UPATTERN_ARGUMENT:
        case DOCOPT__UPATTERN_OPTION:
            munit_assert_string_equal(p.name.it, q.name.it);
            break;
        case DOCOPT__UPATTERN_GROUP:
            munit_assert_size(p.count, ==, q.count);
            for (size_t i=0; i<p.count; i++) {
                if (!upattern_equal(p.child[i], q.child[i])) return false;
            }
            break;
        case DOCOPT__UPATTERN_REPEAT:
            munit_assert_size(p.count, ==, 1);
            munit_assert_size(q.count, ==, 1);
            if (!upattern_equal(p.child[0], q.child[0])) return false;
            break;
        case DOCOPT__UPATTERN_KIND_COUNT:
            munit_assert(false);
            break;
    }
    return true;
}

static bool opattern_equal(Docopt__OPattern p, Docopt__OPattern q) {
    for (size_t i=0; i<DOCOPT__OPTION_KEY_CAPACITY; i++) {
        munit_assert_string_equal(p.key[i].it, q.key[i].it);
    }
    munit_assert_string_equal(p.value.it, q.value.it);
    if (p.def == NULL) {
        munit_assert_null(q.def);
    } else {
        if (q.def == NULL) return false;
        munit_assert_string_equal(p.def, q.def);
    }
    return true;
}

static MunitResult no_argument(const MunitParameter params[], void* user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "  my_program   ";
    Docopt__UPattern expect = {
        .kind = DOCOPT__UPATTERN_ROOT,
        .name = {"my_program"},
        .count = 0,
    };
    Docopt__UPattern p = docopt__compile_upattern(in);

    munit_assert(upattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult command(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "  naval_fate ship create";
    Docopt__UPattern children[] = {
        { .kind = DOCOPT__UPATTERN_COMMAND, .name = {"ship"} },
        { .kind = DOCOPT__UPATTERN_COMMAND, .name = {"create"} },
    };
    Docopt__UPattern expect = {
        .kind = DOCOPT__UPATTERN_ROOT,
        .name = {"naval_fate"},
        .count = ARRAY_LEN(children),
        .child = children,
    };

    Docopt__UPattern p = docopt__compile_upattern(in);
    munit_assert(upattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult argument(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "my_program <host> <port>";
    Docopt__UPattern children[] = {
        { .kind = DOCOPT__UPATTERN_ARGUMENT, .name = {"<host>"} },
        { .kind = DOCOPT__UPATTERN_ARGUMENT, .name = {"<port>"} },
    };
    Docopt__UPattern expect = {
        .kind = DOCOPT__UPATTERN_ROOT,
        .name = {"my_program"},
        .count = ARRAY_LEN(children),
        .child = children,
    };

    Docopt__UPattern p = docopt__compile_upattern(in);
    munit_assert(upattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult option_simple(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "my_program -a -b";
    Docopt__UPattern children[] = {
        { .kind = DOCOPT__UPATTERN_OPTION, .name = {"-a"} },
        { .kind = DOCOPT__UPATTERN_OPTION, .name = {"-b"} },
    };
    Docopt__UPattern expect = {
        .kind = DOCOPT__UPATTERN_ROOT,
        .name = {"my_program"},
        .count = ARRAY_LEN(children),
        .child = children,
    };

    Docopt__UPattern p = docopt__compile_upattern(in);
    munit_assert(upattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult optional_elem(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "my_program [command --option <argument>]";
    Docopt__UPattern children[] = {
        { .kind = DOCOPT__UPATTERN_COMMAND, .name = {"command"} },
        { .kind = DOCOPT__UPATTERN_OPTION,  .name = {"--option"} },
        { .kind = DOCOPT__UPATTERN_ARGUMENT, .name = {"<argument>"} },
    };
    Docopt__UPattern child = {
        .kind = DOCOPT__UPATTERN_GROUP,
        .optional = true,
        .count = ARRAY_LEN(children),
        .child = children,
    };
    Docopt__UPattern expect = {
        .kind = DOCOPT__UPATTERN_ROOT,
        .name = {"my_program"},
        .count = 1,
        .child = &child,
    };

    Docopt__UPattern p = docopt__compile_upattern(in);
    munit_assert(upattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult exclusive(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "my_program go (--up | --down | --left | --right)";
    Docopt__UPattern alts[] = {
        { .kind = DOCOPT__UPATTERN_OPTION, .name = {"--up"} },
        { .kind = DOCOPT__UPATTERN_OPTION, .name = {"--down"} },
        { .kind = DOCOPT__UPATTERN_OPTION, .name = {"--left"} },
        { .kind = DOCOPT__UPATTERN_OPTION, .name = {"--right"} },
    };
    Docopt__UPattern children[] = {
        { .kind = DOCOPT__UPATTERN_COMMAND, .name = {"go"} },
        { .kind = DOCOPT__UPATTERN_GROUP, .optional = false, .alternative = true, .count = ARRAY_LEN(alts), .child = alts },
    };
    Docopt__UPattern expect = {
        .kind = DOCOPT__UPATTERN_ROOT,
        .name = {"my_program"},
        .count = ARRAY_LEN(children),
        .child = children,
    };

    Docopt__UPattern p = docopt__compile_upattern(in);
    munit_assert(upattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult repeat(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "my_program open <file>...";
    Docopt__UPattern child = {
        .kind = DOCOPT__UPATTERN_ARGUMENT,
        .name = {"<file>"},
    };
    Docopt__UPattern children[] = {
        { .kind = DOCOPT__UPATTERN_COMMAND, .name = {"open"} },
        { .kind = DOCOPT__UPATTERN_REPEAT, .count = 1, .child = &child },
    };
    Docopt__UPattern expect = {
        .kind = DOCOPT__UPATTERN_ROOT,
        .name = {"my_program"},
        .count = ARRAY_LEN(children),
        .child = children,
    };

    Docopt__UPattern p = docopt__compile_upattern(in);
    munit_assert(upattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult option_no_arg(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "  --verbose  ";
    Docopt__OPattern expect = {
        .key[0] = {"--verbose"},
        .value = {""},
    };

    Docopt__OPattern p = docopt__compile_opattern(in);
    munit_assert(opattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult option_arg(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "  -o FILE";
    Docopt__OPattern expect = {
        .key[0] = {"-o"},
        .value = {"FILE"},
    };

    Docopt__OPattern p = docopt__compile_opattern(in);
    munit_assert(opattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult option_synonym(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "  -o FILE --output=FILE";
    Docopt__OPattern expect = {
        .key[0] = {"-o"},
        .key[1] = {"--output"},
        .value = {"FILE"},
    };

    Docopt__OPattern p = docopt__compile_opattern(in);
    munit_assert(opattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult option_synonym_comma(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "  -i <file>, --input <file>";
    Docopt__OPattern expect = {
        .key[0] = {"-i"},
        .key[1] = {"--input"},
        .value = {"<file>"},
    };

    Docopt__OPattern p = docopt__compile_opattern(in);
    munit_assert(opattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult option_description(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "-o FILE   Output file.";
    Docopt__OPattern expect = {
        .key[0] = {"-o"},
        .value = {"FILE"},
    };

    Docopt__OPattern p = docopt__compile_opattern(in);
    munit_assert(opattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult option_default(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char *in = "--coefficient=K  The K coefficient [default: 2.95]";
    Docopt__OPattern expect = {
        .key[0] = {"--coefficient"},
        .value = {"K"},
        .def = "2.95",
    };

    Docopt__OPattern p = docopt__compile_opattern(in);
    munit_assert(opattern_equal(expect, p));

    return MUNIT_OK;
}

static MunitResult interpret(const MunitParameter params[], void *user_data_or_fixture) {
    (void) params;
    (void) user_data_or_fixture;

    const char help_message[] = 
        "Naval Fate.\n"
        "\n"
        "Usage:\n"
        "  naval_fate ship create <name>...\n"
        "  naval_fate ship <name> move <x> <y> [--speed=<kn>]\n"
        "  naval_fate ship shoot <x> <y>\n"
        "  naval_fate mine (set|remove) <x> <y> [--moored|--drifting]\n"
        "  naval_fate --help\n"
        "  naval_fate --version\n"
        "\n"
        "Options:\n"
        "  -h --help     Show this screen.\n"
        "  --version     Show version.\n"
        "  --speed=<kn>  Speed in knots [default: 10].\n"
        "  --moored      Moored (anchored) mine.\n"
        "  --drifting    Drifting mine.\n";
    const char *argv[] = {
        "naval_fate",
        "ship",
        "create",
        "beagle",
        "enterprise",
    };
    int argc = ARRAY_LEN(argv);

    Docopt_Match m = docopt_interpret(help_message, argc, argv);
    munit_assert_int(m.count, ==, 5);

    munit_assert_int(m.kind[0], ==, DOCOPT_PROGRAM_NAME);
    munit_assert_string_equal(m.value[0], "naval_fate");

    munit_assert_int(m.kind[1], ==, DOCOPT_SUBCOMMAND);
    munit_assert_string_equal(m.value[1], "ship");

    munit_assert_int(m.kind[2], ==, DOCOPT_SUBCOMMAND);
    munit_assert_string_equal(m.value[2], "create");

    munit_assert_int(m.kind[3], ==, DOCOPT_ARGUMENT);
    munit_assert_string_equal(m.key[3], "<name>");
    munit_assert_string_equal(m.value[3], "beagle");

    munit_assert_int(m.kind[4], ==, DOCOPT_ARGUMENT);
    munit_assert_string_equal(m.key[4], "<name>");
    munit_assert_string_equal(m.value[4], "enterprise");

    return MUNIT_ERROR;
}

MunitTest test_array[] = {
    {
        "/compile/upattern/no_argument",
        no_argument,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/upattern/command",
        command,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/upattern/argument",
        argument,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/upattern/option/simple",
        option_simple,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/upattern/optional",
        optional_elem,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/upattern/exclusive",
        exclusive,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/upattern/repeat",
        repeat,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/opattern/no_argument",
        option_no_arg,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/opattern/argument",
        option_arg,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/opattern/synonym",
        option_synonym,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/opattern/synonym/comma",
        option_synonym_comma,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/opattern/description",
        option_description,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/compile/opattern/description/default",
        option_default,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    {
        "/interpret",
        interpret,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

MunitSuite suite = {
    "/root",
    test_array,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
