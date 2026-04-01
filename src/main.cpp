#include "runtime.hpp"

#include <cstdio>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: yos <program.wasm> [args...]\n");
        return 1;
    }

    // Build argv for wasm: [program_name, args...]
    static char* wasmArgv[256];
    int wasmArgc;

    std::filesystem::path wasmPath(argv[1]);
    static std::string progName = wasmPath.filename().string();

    if (argc == 2) {
        // No extra args - use just the program name
        wasmArgv[0] = progName.data();
        wasmArgc = 1;
    } else {
        // Extra args provided - use them (argv[2] becomes argv[0])
        wasmArgc = argc - 2;
        for (int i = 0; i < wasmArgc; i++) {
            wasmArgv[i] = argv[i + 2];
        }
    }
    wasmArgv[wasmArgc] = nullptr;

    yos::Runtime runtime;
    return runtime.run(argv[1], wasmArgc, wasmArgv);
}
