#include "runtime.hpp"

#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: yos <program.wasm> [args...]\n");
        return 1;
    }

    yos::Runtime runtime;
    // Pass remaining args (argv[2]...) to wasm program
    return runtime.run(argv[1], argc - 2, argv + 2);
}
