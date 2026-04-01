.PHONY: build clean reconfigure busybox

BUILD_DIR := build

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
