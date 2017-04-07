#/bin/bash

gcc -fPIC -c p_mmap.c
gcc -shared -o scm.so p_mmap.o
