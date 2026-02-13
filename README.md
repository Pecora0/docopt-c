# docopt-c

My attempt at using the description language
[docopt](http://docopt.org/)
in C projects.

**This is all work in progress.**

## Idea

./docopt.h is a stb-style single-file-library.
Copy it to your project and include it in a C-file.
You have to define `DOCOPT_IMPLEMENTATION` in one source file.

Calling the function `docopt_interpret` with valid docopt-code
and the command line arguments gives you a
`Docopt_Match` struct that you can query.

Furthermore I plan to add functionality to translate a given docopt-code
to a C-code snippet that you can copy to your project.
That way your code does not depend on the docopt parser on runtime.
