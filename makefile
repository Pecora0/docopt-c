CFLAGS := -Wall -Wextra -Werror

all: build/docopt_util build/test build/gen_testcases

clean:
	rm -r ./build

test: build/docopt_util build/test
	./build/test

gen_testcases: build/gen_testcases
	./build/gen_testcases

build:
	mkdir -p build

build/docopt_util: docopt.h build
	$(CC) $(CFLAGS) -o build/docopt_util -DDOCOPT_UTILITY -x c docopt.h

build/test: test.c docopt.h munit/munit.c build
	$(CC) $(CFLAGS) -o build/test test.c munit/munit.c

build/gen_testcases: gen_testcases.c testcases.docopt build
	$(CC) $(CFLAGS) -o build/gen_testcases gen_testcases.c
