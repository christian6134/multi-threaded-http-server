//iowrapper.h

#pragma once

#include <stdint.h>
#include <sys/types.h>

ssize_t read_n_bytes(int in, char buf[], size_t n);
ssize_t write_n_bytes(int out, char buf[], size_t n);
ssize_t pass_n_bytes(int src, int dst, size_t n);

