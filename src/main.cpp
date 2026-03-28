#include "runtime.hpp"

#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: yos <program.wasm>\n");
        return 1;
    }

    yos::Runtime runtime;
    return runtime.run(argv[1]);
}
