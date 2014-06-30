#!/bin/bash
clang -Xclang -load -Xclang ../../../../Debug+Asserts/lib/Contractor.so -Xclang -plugin -Xclang contractor -c test.c
