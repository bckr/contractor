#!/bin/bash
clang -Xclang -load -Xclang ../../../../Debug+Asserts/lib/PrintFunctionNames.so -Xclang -plugin -Xclang print-fns -c test.c
