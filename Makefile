.PHONY: build clean reconfigure busybox run-tests unit-test build-tests run-busybox-tests

BUILD_DIR := build
WASM_TEST_DIR := $(BUILD_DIR)/tests/unit/wasm

build:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null 2>&1
	@cmake --build $(BUILD_DIR) -j$$(nproc)

clean:
	@rm -rf $(BUILD_DIR)

reconfigure:
	@rm -rf $(BUILD_DIR)
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@cmake --build $(BUILD_DIR) -j$$(nproc)

busybox:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
	@cmake --build $(BUILD_DIR) -j$$(nproc) 2>&1 | tail -3
	@cmake -DWASI_CC=$(BUILD_DIR)/_deps/wasi-sdk/bin/clang \
		-DWASI_SYSROOT=$(BUILD_DIR)/_deps/wasi-sdk/share/wasi-sysroot \
		-DBUSYBOX_SOURCE_DIR=$(BUILD_DIR)/_deps/busybox-src \
		-DYOS_SOURCE_DIR=$(CURDIR) \
		-DOUTPUT_DIR=$(BUILD_DIR)/wasm \
		-DCODEGEN_DIR=$(BUILD_DIR)/generated \
		-P cmake/build-busybox.cmake

# Build unit tests (WASM binaries + test runner)
build-tests:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null 2>&1
	@cmake --build $(BUILD_DIR) --target run_unit_tests --target unit_test_wasm -j$$(nproc)

# Run all unit tests
run-tests: build-tests
	@$(BUILD_DIR)/tests/unit/run_unit_tests $(WASM_TEST_DIR)/*.wasm

# Run a specific unit test (usage: make unit-test T=test_write_exit)
unit-test: build-tests
	@$(BUILD_DIR)/tests/unit/run_unit_tests $(WASM_TEST_DIR)/$(T).wasm

# Run busybox command tests
run-busybox-tests: build
	@chmod +x tests/busybox/run-commands.sh
	@tests/busybox/run-commands.sh
