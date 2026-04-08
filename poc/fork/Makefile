# Makefile for fork POC

WASM3_INC = ../../build/_deps/wasm3-src/source
WASM3_LIB = ../../build/libwasm3.a

CC = gcc
CFLAGS = -Wall -g -I$(WASM3_INC)
LDFLAGS = -lpthread -lm

TESTS = test1_simple test2_locals test3_multiple test4_nested test5_loop test6_stress test7_deep

.PHONY: all clean test $(TESTS)

all: runtime

runtime: runtime.c fork.c fork.h $(WASM3_LIB)
	$(CC) $(CFLAGS) -o $@ runtime.c fork.c $(WASM3_LIB) $(LDFLAGS)

%.wasm: %_raw.wasm
	wasm-opt --asyncify --pass-arg=asyncify-imports@env.fork -o $@ $<

%_raw.wasm: %.c
	clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export=_start -Wl,--allow-undefined -o $@ $<

test1_simple: runtime test1_simple.wasm
	@echo "=== TEST1: Simple fork ==="
	@./runtime test1_simple.wasm

test2_locals: runtime test2_locals.wasm
	@echo "=== TEST2: Local variables ==="
	@./runtime test2_locals.wasm

test3_multiple: runtime test3_multiple.wasm
	@echo "=== TEST3: Multiple forks ==="
	@./runtime test3_multiple.wasm

test4_nested: runtime test4_nested.wasm
	@echo "=== TEST4: Nested forks ==="
	@./runtime test4_nested.wasm

test5_loop: runtime test5_loop.wasm
	@echo "=== TEST5: Fork in loop ==="
	@./runtime test5_loop.wasm

test6_stress: runtime test6_stress.wasm
	@echo "=== TEST6: Stress test (100 forks) ==="
	@./runtime test6_stress.wasm

test7_deep: runtime test7_deep.wasm
	@echo "=== TEST7: Deep nested forks ==="
	@./runtime test7_deep.wasm

test: $(TESTS)
	@echo "=== ALL TESTS COMPLETE ==="

clean:
	rm -f runtime *.wasm
