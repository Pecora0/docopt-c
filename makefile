all: run

run: docopt_util test
	./test

docopt_util: docopt.h
	gcc -Wall -Wextra -Werror -o docopt_util -DDOCOPT_UTILITY -x c docopt.h

test: test.c docopt.h munit/munit.c
	gcc -Wall -Wextra -Werror -o test test.c munit/munit.c
