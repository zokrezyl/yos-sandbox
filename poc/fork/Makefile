# Makefile for fork POC

WASM3_INC = ../../build/_deps/wasm3-src/source
WASM3_LIB = ../../build/libwasm3.a

CC = gcc
CFLAGS = -Wall -g -I$(WASM3_INC)
LDFLAGS = -lpthread -lm

.PHONY: all clean run

all: runtime test_fork.wasm

runtime: runtime.c $(WASM3_LIB)
	$(CC) $(CFLAGS) -o $@ $< $(WASM3_LIB) $(LDFLAGS)

test_fork_raw.wasm: test_fork.c
	clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export=_start -Wl,--allow-undefined -o $@ $<

test_fork.wasm: test_fork_raw.wasm
	wasm-opt --asyncify --pass-arg=asyncify-imports@env.fork -o $@ $<

run: runtime test_fork.wasm
	./runtime

clean:
	rm -f runtime test_fork_raw.wasm test_fork.wasm
