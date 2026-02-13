all: run

run: build/docopt_util build/test
	./build/test

build/docopt_util: docopt.h
	gcc -Wall -Wextra -Werror -o build/docopt_util -DDOCOPT_UTILITY -x c docopt.h

build/test: test.c docopt.h munit/munit.c
	gcc -Wall -Wextra -Werror -o build/test test.c munit/munit.c
