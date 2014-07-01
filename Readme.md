Contractor
============

A Plugin for the Clang Compiler to automatically add assertions to C-Sourcefiles based on their Design by Contract specification.

Instructions
------------
1. Checkout and build Clang using [these instructions](http://clang.llvm.org/get_started.html)
2. Once LLVM is build, clone the Contractor repository into `[PATH_TO_LLVM]/tools/clang/examples/`
3. Use the provided Makefile to build the Contractor Plugin

Testing
------------

To test the Plugin simply run `make test`. This will transform the file `test.c` and print the results to the console. To test the transformation of a polymorphic class run `make test_inheritance`. The latter will modify the file `test.cpp`.
