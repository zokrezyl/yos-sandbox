# Build busybox targeting wasm32-wasip1
# Called as cmake -P script with:
#   WASI_CC, WASI_SYSROOT, BUSYBOX_SOURCE_DIR, YOS_SOURCE_DIR, OUTPUT_DIR

get_filename_component(OUTPUT_DIR_ABS ${OUTPUT_DIR} ABSOLUTE)
set(BB_BUILD "${OUTPUT_DIR_ABS}/bb-build")
file(MAKE_DIRECTORY ${BB_BUILD})
file(MAKE_DIRECTORY ${OUTPUT_DIR_ABS})

# Copy busybox source to build dir (busybox builds in-tree)
if(NOT EXISTS "${BB_BUILD}/Makefile")
    message(STATUS "Copying busybox source to ${BB_BUILD}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${BUSYBOX_SOURCE_DIR} ${BB_BUILD})
endif()

# Get absolute paths
get_filename_component(WASI_CC_ABS ${WASI_CC} ABSOLUTE)
get_filename_component(YOS_SRC_ABS ${YOS_SOURCE_DIR} ABSOLUTE)
get_filename_component(CODEGEN_DIR ${CODEGEN_DIR} ABSOLUTE)

# Generate CC wrapper from template (handles both compiling and linking)
configure_file("${YOS_SRC_ABS}/cmake/wasm-cc.sh.in" "${BB_BUILD}/wasm-cc" @ONLY)
execute_process(COMMAND chmod +x "${BB_BUILD}/wasm-cc")

# Compile yos-stubs.o
message(STATUS "Compiling yos-stubs.o...")
execute_process(
    COMMAND ${BB_BUILD}/wasm-cc -c ${YOS_SRC_ABS}/wasm-stubs/yos-stubs.c -o ${BB_BUILD}/yos-stubs.o
    RESULT_VARIABLE rc
    ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Failed to compile yos-stubs.c:\n${err}")
endif()


# Step 1: allnoconfig
message(STATUS "Configuring busybox (allnoconfig)...")
execute_process(
    COMMAND make -C ${BB_BUILD} allnoconfig HOSTCC=cc CC=${BB_BUILD}/wasm-cc
    OUTPUT_QUIET ERROR_QUIET
)

# Step 2: enable applets incrementally
# Busybox applets to enable
set(BB_ENABLES
    # Core utilities
    "CONFIG_ECHO=y"
    "CONFIG_TRUE=y"
    "CONFIG_FALSE=y"
    "CONFIG_CAT=y"
    "CONFIG_LS=y"
    "CONFIG_PWD=y"
    "CONFIG_TEST=y"
    "CONFIG_TEST1=y"
    "CONFIG_TEST2=y"
    "CONFIG_PRINTF=y"
    "CONFIG_YES=y"
    "CONFIG_SEQ=y"
    "CONFIG_SLEEP=y"
    "CONFIG_USLEEP=y"

    # File operations
    "CONFIG_CP=y"
    "CONFIG_MV=y"
    "CONFIG_RM=y"
    "CONFIG_MKDIR=y"
    "CONFIG_RMDIR=y"
    "CONFIG_TOUCH=y"
    "CONFIG_LN=y"
    "CONFIG_CHMOD=y"
    "CONFIG_CHOWN=y"
    "CONFIG_STAT=y"
    "CONFIG_READLINK=y"
    "CONFIG_REALPATH=y"
    "CONFIG_BASENAME=y"
    "CONFIG_DIRNAME=y"
    # CONFIG_MKTEMP needs mktemp/mkdtemp

    # Text processing
    "CONFIG_GREP=y"
    "CONFIG_EGREP=y"
    "CONFIG_FGREP=y"
    "CONFIG_SED=y"
    "CONFIG_AWK=y"
    "CONFIG_HEAD=y"
    "CONFIG_TAIL=y"
    "CONFIG_WC=y"
    "CONFIG_SORT=y"
    "CONFIG_UNIQ=y"
    "CONFIG_CUT=y"
    "CONFIG_TR=y"
    "CONFIG_TEE=y"
    "CONFIG_XARGS=y"
    "CONFIG_DIFF=y"
    "CONFIG_CMP=y"
    "CONFIG_COMM=y"
    "CONFIG_FOLD=y"
    "CONFIG_PASTE=y"
    "CONFIG_EXPAND=y"
    "CONFIG_UNEXPAND=y"
    "CONFIG_NL=y"
    "CONFIG_OD=y"
    "CONFIG_HEXDUMP=y"
    "CONFIG_XXD=y"
    "CONFIG_BASE64=y"
    "CONFIG_MD5SUM=y"
    "CONFIG_SHA1SUM=y"
    "CONFIG_SHA256SUM=y"
    "CONFIG_SHA512SUM=y"

    # System info
    "CONFIG_DATE=y"
    "CONFIG_UNAME=y"
    # CONFIG_HOSTNAME needs sethostname
    "CONFIG_WHOAMI=y"
    "CONFIG_ID=y"
    "CONFIG_GROUPS=y"
    "CONFIG_ENV=y"
    "CONFIG_PRINTENV=y"
    "CONFIG_EXPR=y"
    # CONFIG_NPROC needs sched_getaffinity

    # Process management - uses /proc filesystem
    "CONFIG_PS=y"
    "CONFIG_FEATURE_PS_LONG=y"
    "CONFIG_FEATURE_PS_WIDE=y"
    "CONFIG_KILL=y"
    "CONFIG_KILLALL=y"
    "CONFIG_PGREP=y"
    "CONFIG_PKILL=y"
    "CONFIG_PIDOF=y"
    "CONFIG_PSTREE=y"
    "CONFIG_UPTIME=y"

    # File finding
    "CONFIG_FIND=y"
    "CONFIG_WHICH=y"
    "CONFIG_WHEREIS=y"

    # Archive - TAR needs makedev
    # "CONFIG_TAR=y"
    "CONFIG_GZIP=y"
    "CONFIG_GUNZIP=y"
    "CONFIG_ZCAT=y"
    "CONFIG_BZIP2=y"
    "CONFIG_BUNZIP2=y"
    "CONFIG_BZCAT=y"
    "CONFIG_UNZIP=y"

    # Misc
    "CONFIG_CLEAR=y"
    "CONFIG_RESET=y"
    # CONFIG_TIME needs rusage
    "CONFIG_TIMEOUT=y"
    "CONFIG_NOHUP=y"
    # CONFIG_NICE needs PRIO_PROCESS
    # CONFIG_WATCH needs select/poll
    "CONFIG_STRINGS=y"
    "CONFIG_DD=y"
    "CONFIG_SPLIT=y"
    "CONFIG_INSTALL=y"
    # Shell
    "CONFIG_ASH=y"
    "CONFIG_ASH_ALIAS=y"
    "CONFIG_ASH_BASH_COMPAT=y"
    "CONFIG_ASH_INTERNAL_GLOB=y"
    "CONFIG_ASH_OPTIMIZE_FOR_SIZE=y"
    # Required infra
    "CONFIG_SHOW_USAGE=y"
    "CONFIG_FEATURE_VERBOSE_USAGE=y"
    "CONFIG_LONG_OPTS=y"
    "CONFIG_FEATURE_PREFER_APPLETS=y"
    "CONFIG_FEATURE_SH_STANDALONE=y"
    "CONFIG_FEATURE_SH_NOFORK=y"
    # Disable problematic features
    "CONFIG_FEATURE_SUID=n"
    "CONFIG_FEATURE_SUID_CONFIG=n"
    "CONFIG_ASH_JOB_CONTROL=n"
    "CONFIG_FEATURE_EDITING=n"
    "CONFIG_PLATFORM_LINUX=n"
    "CONFIG_LFS=y"
    "CONFIG_CROSS_COMPILER_PREFIX=\"\""
    "CONFIG_EXTRA_CFLAGS=\"\""
    "CONFIG_EXTRA_LDLIBS=\"\""
    "CONFIG_EXTRA_LDFLAGS=\"\""
    "CONFIG_SHA256_HWACCEL=n"
    "CONFIG_SHA1_HWACCEL=n"
    "CONFIG_MD5_SIZE_VS_SPEED=2"
)

# Append enables to .config
file(READ "${BB_BUILD}/.config" BB_CONFIG)
foreach(opt ${BB_ENABLES})
    string(REGEX REPLACE "([^=]+)=.*" "\\1" OPT_NAME ${opt})
    # Remove existing line for this option (whether =y or not set)
    string(REGEX REPLACE "${OPT_NAME}=[^\n]*\n" "" BB_CONFIG "${BB_CONFIG}")
    string(REGEX REPLACE "# ${OPT_NAME} is not set\n" "" BB_CONFIG "${BB_CONFIG}")
    set(BB_CONFIG "${BB_CONFIG}${opt}\n")
endforeach()
file(WRITE "${BB_BUILD}/.config" "${BB_CONFIG}")

# oldconfig to resolve deps
execute_process(
    COMMAND make -C ${BB_BUILD} oldconfig HOSTCC=cc CC=${BB_BUILD}/wasm-cc
    INPUT_FILE /dev/null
    OUTPUT_QUIET ERROR_QUIET
)

# Disable --gc-sections in trylink (we need all symbols for wasm)
execute_process(
    COMMAND sed -i
        -e "s/GC_SECTIONS=.*/GC_SECTIONS=\"\"/"
        "${BB_BUILD}/scripts/trylink"
)

# Replace x86 assembly files with empty C stubs (can't compile to wasm)
file(GLOB X86_ASM "${BB_BUILD}/libbb/*.S")
foreach(f ${X86_ASM})
    string(REGEX REPLACE "\\.S$" ".c" c_file ${f})
    file(WRITE ${c_file} "/* x86 asm removed for wasm build */\n")
    file(REMOVE ${f})
endforeach()

# Create stub libraries (libm, libpthread, etc.) - empty archives
# wasm-ld needs these to exist even if empty
set(STUB_LIB_DIR "${BB_BUILD}/stub-libs")
file(MAKE_DIRECTORY ${STUB_LIB_DIR})

# Create empty .o file for stub libs
file(WRITE "${STUB_LIB_DIR}/stub.c" "/* empty stub */\n")
execute_process(
    COMMAND ${WASI_CC_ABS} --target=wasm32 -nostdlib -c
        ${STUB_LIB_DIR}/stub.c -o ${STUB_LIB_DIR}/stub.o
)

# Create stub libraries
foreach(LIB m pthread dl rt crypt resolv)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env ar rcs ${STUB_LIB_DIR}/lib${LIB}.a ${STUB_LIB_DIR}/stub.o
    )
endforeach()

# BB_VER comes from Makefile, passed as -D. Check it's in autoconf.h
# AUTOCONF_TIMESTAMP is in autoconf.h, no patching needed.

# Step 3: Build
message(STATUS "Building busybox...")
execute_process(
    COMMAND make -C ${BB_BUILD} -j4 -k V=1
        HOSTCC=cc
        CC=${BB_BUILD}/wasm-cc
        SKIP_STRIP=y
    RESULT_VARIABLE rc
    OUTPUT_FILE "${OUTPUT_DIR_ABS}/busybox-build-stdout.log"
    ERROR_FILE "${OUTPUT_DIR_ABS}/busybox-build-stderr.log"
    TIMEOUT 300
)

if(rc EQUAL 0)
    file(COPY "${BB_BUILD}/busybox_unstripped" DESTINATION ${OUTPUT_DIR})
    file(RENAME "${OUTPUT_DIR_ABS}/busybox_unstripped" "${OUTPUT_DIR_ABS}/busybox.wasm")
    # Normalize LEB encodings with wasm-opt (wasm3 chokes on non-canonical encodings)
    find_program(WASM_OPT wasm-opt)
    if(WASM_OPT)
        message(STATUS "Optimizing with wasm-opt...")
        execute_process(
            COMMAND ${WASM_OPT} -O1 "${OUTPUT_DIR_ABS}/busybox.wasm" -o "${OUTPUT_DIR_ABS}/busybox.wasm"
            RESULT_VARIABLE opt_rc
        )
        if(NOT opt_rc EQUAL 0)
            message(WARNING "wasm-opt failed, using unoptimized binary")
        endif()
    endif()
    execute_process(COMMAND ls -lh "${OUTPUT_DIR_ABS}/busybox.wasm" OUTPUT_VARIABLE sz)
    message(STATUS "SUCCESS: ${sz}")
else()
    # Show last errors
    file(READ "${OUTPUT_DIR_ABS}/busybox-build-stderr.log" ERRS)
    string(LENGTH "${ERRS}" ERRLEN)
    if(ERRLEN GREATER 2000)
        math(EXPR START "${ERRLEN} - 2000")
        string(SUBSTRING "${ERRS}" ${START} 2000 ERRS)
    endif()
    message(STATUS "Build failed (rc=${rc}). Last errors:\n${ERRS}")
    message(STATUS "Full logs: ${OUTPUT_DIR_ABS}/busybox-build-stderr.log")
endif()
