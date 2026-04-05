#!/usr/bin/env python3
"""
Generate passthrough libc wrappers and unit tests from libc.yaml
Output:
  - build/generated/libc-wrappers.c - C++ passthrough wrappers
  - build/generated/libc-wrappers.h - Header declarations
  - tests/unit/wasm-src/libc/ - One test file per function
"""

import yaml
import os
import sys

# Default test values per type
DEFAULT_VALUES = {
    'i32': '0',
    'u32': '0',
    'i64': '0LL',
    'u64': '0ULL',
    'str': '"test"',
    'ptr': 'NULL',
    'handle': 'NULL',
    'void': '',
}

# Type mappings for C code
C_TYPES = {
    'i32': 'int32_t',
    'u32': 'uint32_t',
    'i64': 'int64_t',
    'u64': 'uint64_t',
    'str': 'const char*',
    'ptr': 'void*',
    'handle': 'void*',
    'void': 'void',
}

# m3 signature chars
M3_SIG = {
    'i32': 'i',
    'u32': 'i',
    'i64': 'I',
    'u64': 'I',
    'str': '*',
    'ptr': '*',
    'handle': 'i',  # handle is raw int, NOT memory pointer
    'func_ptr': 'i',  # function pointer is table index (i32)
    'void': 'v',
}


def load_libc_yaml(path):
    """Load and parse libc.yaml"""
    with open(path) as f:
        content = f.read()

    # Parse YAML-like format (it's not strict YAML due to comments)
    functions = []
    current = None
    current_header = None

    for line in content.split('\n'):
        line = line.rstrip()
        if not line:
            continue

        # Track current header from "# From <header.h>" comments
        if line.startswith('# From <'):
            current_header = line.split('<')[1].split('>')[0]
            continue
        if line.startswith('#'):
            continue

        if line.strip().startswith('- name:'):
            if current:
                functions.append(current)
            name = line.split('name:')[1].strip()
            current = {'name': name, 'params': [], 'returns': 'i32', 'returns_c': 'int', 'variadic': False, 'header': current_header}
        elif current:
            if 'params:' in line:
                # Parse params: [{name: yaml_type: c_type}, ...]
                params_str = line.split('params:')[1].strip()
                if params_str != '[]':
                    params_str = params_str.strip('[]')
                    for p in params_str.split('}, {'):
                        p = p.strip('{} ')
                        parts = p.split(':')
                        if len(parts) >= 3:
                            pname = parts[0].strip()
                            yaml_type = parts[1].strip()
                            c_type = ':'.join(parts[2:]).strip()  # c_type may contain colons
                            current['params'].append((pname, yaml_type, c_type))
                        elif len(parts) == 2:
                            pname = parts[0].strip()
                            yaml_type = parts[1].strip()
                            current['params'].append((pname, yaml_type, C_TYPES.get(yaml_type, 'int')))
            elif line.strip().startswith('returns_c:'):
                current['returns_c'] = line.split('returns_c:')[1].strip()
            elif line.strip().startswith('returns:'):
                current['returns'] = line.split('returns:')[1].strip()
            elif 'variadic: true' in line:
                current['variadic'] = True
            elif 'struct_return:' in line:
                # Parse struct_return: {singleton: true/false, field: X, field_type: Y, size: Z}
                sr_str = line.split('struct_return:')[1].strip().strip('{}')
                sr = {}
                for part in sr_str.split(', '):
                    if ':' in part:
                        k, v = part.split(':', 1)
                        k = k.strip()
                        v = v.strip()
                        if v == 'true':
                            sr[k] = True
                        elif v == 'false':
                            sr[k] = False
                        elif v.isdigit():
                            sr[k] = int(v)
                        else:
                            sr[k] = v
                current['struct_return'] = sr

    if current:
        functions.append(current)

    return functions

def generate_wrapper(func):
    """Generate C++ wrapper for a function"""
    name = func['name']
    params = func['params']
    ret = func['returns']
    variadic = func.get('variadic', False)
    struct_return = func.get('struct_return')

    # Variadic functions need special handling
    if variadic:
        return generate_varargs_wrapper(func)

    # Get C return type
    ret_c = func.get('returns_c', C_TYPES.get(ret, 'int'))

    # Check and fix parameters
    fixed_params = []
    has_array_type_param = False
    for p in params:
        pname = p[0]
        yaml_type = p[1]
        c_type = p[2] if len(p) > 2 else C_TYPES.get(yaml_type, 'int')

        # Convert array types to pointers: int[10] -> int*
        if '[' in c_type:
            c_type = c_type.split('[')[0].strip() + '*'

        # Function pointer params are table indices (i32) in WASM
        # Mark them for special handling
        is_func_ptr = '(*)' in c_type or '(*' in c_type
        if is_func_ptr:
            yaml_type = 'func_ptr'  # special marker

        # Detect array typedef params (jmp_buf, sigjmp_buf, etc - passed by value but are arrays)
        if c_type in ('jmp_buf', 'sigjmp_buf', '__jmp_buf', '__gnuc_va_list', 'va_list'):
            has_array_type_param = True

        fixed_params.append((pname, yaml_type, c_type))

    params = fixed_params

    # Array types passed by value can't work with m3ApiGetArg - need pointer
    if has_array_type_param:
        return None, None, False, f"array type param (jmp_buf/va_list)"

    # Function pointer returns - return as i32 (table index)
    is_func_ptr_return = '(*)' in ret_c or '(*' in ret_c or ret_c in ('__sighandler_t', 'sighandler_t')

    # Name conflicts with struct types
    if name in ('flock',):  # flock conflicts with struct flock
        return None, None, False, "name conflicts with struct"




    # Complex params are passed as pointers in WASM ABI - mark for dereference when calling
    complex_params = set()
    for i, p in enumerate(params):
        if '_Complex' in p[2]:
            complex_params.add(p[0])
            # Mark as pointer type for m3 signature
            params[i] = (p[0], 'ptr', p[2] + ' *')  # pass by pointer

    # Build m3 signature - handle struct returns
    if struct_return and not struct_return.get('singleton'):
        # Non-singleton struct: sret convention - first param is result pointer, return void
        ret_sig = 'v'
        param_sigs = '*' + ''.join([M3_SIG.get(p[1], 'i') for p in params])
    else:
        ret_sig = M3_SIG.get(ret, 'i')
        param_sigs = ''.join([M3_SIG.get(p[1], 'i') for p in params])
    m3_sig = f"{ret_sig}({param_sigs})"

    # Build wrapper function
    lines = []
    lines.append(f'm3ApiRawFunction(libc_{name}) {{')

    # Handle struct return types
    if struct_return:
        if struct_return.get('singleton'):
            # Singleton struct: return the scalar field directly
            m3_ret = 'uint32_t' if struct_return['size'] <= 4 else 'uint64_t'
            lines.append(f'    m3ApiReturnType({m3_ret});')
        else:
            # Non-singleton: sret - get result pointer as first arg
            lines.append(f'    m3ApiGetArgMem(void*, _sret);')
    elif is_func_ptr_return:
        # Function pointer return - return as i32 (table index)
        lines.append(f'    m3ApiReturnType(uint32_t);')
    elif ret == 'handle':
        # Handle returns as uint32_t (opaque handle stored as integer)
        lines.append(f'    m3ApiReturnType(uint32_t);')
    elif ret != 'void':
        m3_ret = C_TYPES.get(ret, 'int32_t')
        lines.append(f'    m3ApiReturnType({m3_ret});')

    # Get arguments using actual C types
    for p in params:
        pname = p[0]
        yaml_type = p[1]
        c_type = p[2] if len(p) > 2 else C_TYPES.get(yaml_type, 'int')

        if yaml_type in ('str', 'ptr'):
            lines.append(f'    m3ApiGetArgMem({c_type}, {pname});')
        elif yaml_type == 'handle':
            lines.append(f'    m3ApiGetArg(uint32_t, _{pname}_raw);')
            lines.append(f'    {c_type} {pname} = ({c_type})(uintptr_t)_{pname}_raw;')
        elif yaml_type == 'func_ptr':
            # Function pointer is table index (i32) - callback not implemented yet
            lines.append(f'    m3ApiGetArg(uint32_t, _{pname}_idx);')
            lines.append(f'    (void)_{pname}_idx; // TODO: implement callback trampoline')
        else:
            lines.append(f'    m3ApiGetArg({c_type}, {pname});')

    # Call native function and handle return
    # Build arg expressions with special handling
    arg_exprs = []
    for p in params:
        pname, yaml_type, c_type = p
        if pname in complex_params:
            arg_exprs.append(f'*{pname}')  # dereference pointer
        elif yaml_type == 'func_ptr':
            arg_exprs.append('NULL')  # placeholder - callback not implemented
        else:
            arg_exprs.append(pname)
    args = ', '.join(arg_exprs)

    # Some function names need special handling
    call_name = name

    if struct_return:
        if struct_return.get('singleton'):
            # Singleton: call function, extract field, return as scalar
            field = struct_return['field']
            lines.append(f'    {ret_c} _result = {call_name}({args});')
            lines.append(f'    m3ApiReturn(_result.{field});')
        else:
            # Non-singleton: call function, copy result to sret pointer
            size = struct_return['size']
            lines.append(f'    {ret_c} _result = {call_name}({args});')
            lines.append(f'    memcpy(_sret, &_result, {size});')
            lines.append(f'    m3ApiSuccess();')
    elif ret == 'void':
        lines.append(f'    {call_name}({args});')
        lines.append(f'    m3ApiSuccess();')
    elif ret == 'handle':
        lines.append(f'    {ret_c} _result = {call_name}({args});')
        lines.append(f'    m3ApiReturn((uint32_t)(uintptr_t)_result);')
    elif is_func_ptr_return:
        # Function pointer return - cast to uint32_t (placeholder, can't convert host func to wasm table)
        lines.append(f'    {ret_c} _result = {call_name}({args});')
        lines.append(f'    m3ApiReturn((uint32_t)(uintptr_t)_result);')
    else:
        lines.append(f'    {ret_c} _result = {call_name}({args});')
        lines.append(f'    m3ApiReturn(_result);')

    lines.append('}')

    return '\n'.join(lines), m3_sig, False, None


def generate_varargs_wrapper(func):
    """Generate GENERIC wrapper for ANY variadic function based on signature.
    Pattern: get fixed params + va_ptr, pack into array, call via trampoline.
    Calls the SAME function (printf calls printf, NOT vprintf).
    """
    name = func['name']
    params = func['params']
    ret = func['returns']
    ret_c = func.get('returns_c', C_TYPES.get(ret, 'int'))

    # Build m3 signature: fixed params + va_ptr (i32)
    param_sigs = ''.join([M3_SIG.get(p[1], 'i') for p in params])
    param_sigs += 'i'  # va_ptr
    ret_sig = M3_SIG.get(ret, 'i')
    m3_sig = f"{ret_sig}({param_sigs})"

    lines = []
    lines.append(f'm3ApiRawFunction(libc_{name}) {{')

    # Return type
    if ret != 'void':
        lines.append(f'    m3ApiReturnType({C_TYPES.get(ret, "int32_t")});')

    # Get fixed params from signature
    for p in params:
        pname = p[0]
        yaml_type = p[1]
        c_type = p[2] if len(p) > 2 else C_TYPES.get(yaml_type, 'int')

        if yaml_type in ('str', 'ptr'):
            lines.append(f'    m3ApiGetArgMem({c_type}, {pname});')
        elif yaml_type == 'handle':
            lines.append(f'    m3ApiGetArg(uint32_t, _{pname}_handle);')
            lines.append(f'    {c_type} {pname} = ({c_type})(uintptr_t)_{pname}_handle;')
        else:
            lines.append(f'    m3ApiGetArg({c_type}, {pname});')

    # Get va_ptr (WASM adds this as last param for varargs)
    lines.append('    m3ApiGetArg(uint32_t, _va_ptr);')

    # Get WASM memory (use _wasm_mem to avoid conflict with m3Api's _mem)
    lines.append('    uint32_t _wasm_mem_size; uint8_t* _wasm_mem = m3_GetMemory(runtime, &_wasm_mem_size, 0);')

    # Pack fixed args + varargs into array for trampoline
    lines.append('    uint64_t _args[32];')
    lines.append(f'    int _argc = 0;')

    # Pack fixed params
    for p in params:
        pname = p[0]
        lines.append(f'    _args[_argc++] = (uint64_t)(uintptr_t){pname};')

    # Pack varargs from WASM memory (each is 4 bytes in WASM, promote to 64-bit)
    lines.append('    uint32_t _va_off = 0;')
    lines.append('    for (int _i = 0; _i < 16 && _argc < 32; _i++) {')
    lines.append('        uint32_t _v = *(uint32_t*)(_wasm_mem + _va_ptr + _va_off);')
    lines.append('        _args[_argc++] = (uint64_t)_v;')
    lines.append('        _va_off += 4;')
    lines.append('    }')

    # Call via trampoline
    if ret == 'void':
        lines.append(f'    call_native_varargs((void*){name}, _args, _argc);')
        lines.append('    m3ApiSuccess();')
    else:
        lines.append(f'    {ret_c} _result = ({ret_c})(uintptr_t)call_native_varargs((void*){name}, _args, _argc);')
        lines.append(f'    m3ApiReturn(_result);')

    lines.append('}')
    return '\n'.join(lines), m3_sig, False, None


def generate_test(func):
    """Generate unit test for a function"""
    name = func['name']
    params = func['params']
    ret = func['returns']
    variadic = func.get('variadic', False)
    struct_return = func.get('struct_return')
    ret_c = func.get('returns_c', 'int')

    # Build test call with default values
    args = []
    for p in params:
        ptype = p[1]  # yaml_type
        args.append(DEFAULT_VALUES.get(ptype, '0'))

    # Variadic functions: add NULL terminator or appropriate vararg
    if variadic:
        # For printf-family, format string is usually first/second param
        # Just call with fixed args, no extra varargs
        pass

    args_str = ', '.join(args)

    lines = []
    lines.append(f'// Unit test for {name}')
    lines.append(f'// Auto-generated by generate-libc-calls.py')
    lines.append('')
    lines.append('typedef int int32_t;')
    lines.append('typedef unsigned int uint32_t;')
    lines.append('typedef long long int64_t;')
    lines.append('typedef unsigned long long uint64_t;')
    lines.append('#define NULL ((void*)0)')
    lines.append('')

    # Handle struct return types
    if struct_return and not struct_return.get('singleton'):
        # Non-singleton struct return: sret convention
        # Define struct type and use void return with pointer first arg
        size = struct_return['size']
        lines.append(f'// Struct return type (size={size})')
        lines.append(f'typedef struct {{ char _data[{size}]; }} {ret_c};')
        lines.append('')
        lines.append('// Import from yos - sret convention: void return, struct ptr first')
        wasm_params = ', '.join([f'{C_TYPES.get(p[1], "int32_t")} {p[0]}' for p in params])
        if wasm_params:
            lines.append(f'extern void {name}({ret_c}* _sret, {wasm_params});')
        else:
            lines.append(f'extern void {name}({ret_c}* _sret);')
        lines.append('extern void _exit(int status);')
        lines.append('')
        lines.append('void _start(void) {')
        lines.append(f'    {ret_c} result;')
        if args_str:
            lines.append(f'    {name}(&result, {args_str});')
        else:
            lines.append(f'    {name}(&result);')
        lines.append('    (void)result;')
        lines.append('    _exit(0);')
    else:
        lines.append('// Import from yos - use WASM types')
        # Use WASM types for declaration
        wasm_ret = C_TYPES.get(ret, 'int32_t')
        wasm_params = ', '.join([f'{C_TYPES.get(p[1], "int32_t")} {p[0]}' for p in params]) or 'void'
        lines.append(f'extern {wasm_ret} {name}({wasm_params});')
        lines.append('extern void _exit(int status);')
        lines.append('')
        lines.append('void _start(void) {')

        if ret == 'void':
            lines.append(f'    {name}({args_str});')
            lines.append('    _exit(0);')
        else:
            lines.append(f'    {wasm_ret} result = {name}({args_str});')
            lines.append('    (void)result;')
            lines.append('    _exit(0);')

    lines.append('}')

    return '\n'.join(lines)

def main():
    libc_yaml = 'build/generated/libc.yaml'
    if not os.path.exists(libc_yaml):
        print(f"Error: {libc_yaml} not found. Run extract-signatures.py first.", file=sys.stderr)
        sys.exit(1)

    # Load glibc exports from build output
    glibc_exports_file = 'build/generated/glibc-exports.txt'
    if not os.path.exists(glibc_exports_file):
        print(f"Error: {glibc_exports_file} not found. Run 'make glibc-exports' first.", file=sys.stderr)
        sys.exit(1)

    with open(glibc_exports_file) as f:
        all_exports = [line.strip() for line in f if line.strip()]
    # Filter out internal __ prefixed functions
    glibc_exports = set(name for name in all_exports if not name.startswith('__'))
    print(f"Loaded {len(glibc_exports)} glibc exports (filtered {len(all_exports) - len(glibc_exports)} internal __ functions)")

    print(f"Loading {libc_yaml}...")
    functions = load_libc_yaml(libc_yaml)
    print(f"Loaded {len(functions)} functions from headers")

    # Output directories - ALL generated code goes to build/
    os.makedirs('build/generated', exist_ok=True)
    os.makedirs('build/generated/smoke-tests', exist_ok=True)

    # Generate wrappers
    wrappers = []
    link_calls = []
    skipped = {}  # reason -> list of names
    used_headers = set()  # Collect headers for included functions

    for func in functions:
        # Only wrap functions that are actually exported from glibc
        if func['name'] not in glibc_exports:
            if 'not in glibc exports' not in skipped:
                skipped['not in glibc exports'] = []
            skipped['not in glibc exports'].append(func['name'])
            continue
        wrapper, m3_sig, _, skip_reason = generate_wrapper(func)
        if wrapper is None:
            if skip_reason not in skipped:
                skipped[skip_reason] = []
            skipped[skip_reason].append(func['name'])
            continue
        wrappers.append(wrapper)
        link_calls.append(f'    m3_LinkRawFunction(module, "env", "{func["name"]}", "{m3_sig}", libc_{func["name"]});')
        # Track header for this function
        if func.get('header'):
            used_headers.add(func['header'])

    # Write wrappers header
    with open('build/generated/libc-wrappers.h', 'w') as f:
        f.write('#ifndef LIBC_WRAPPERS_H\n')
        f.write('#define LIBC_WRAPPERS_H\n\n')
        f.write('// Auto-generated libc passthrough wrappers\n')
        f.write('// Generated by tools/generate-libc-calls.py\n\n')
        f.write('#include "wasm3.h"\n\n')
        f.write('void linkLibcFunctions(IM3Module module);\n\n')
        f.write('#endif // LIBC_WRAPPERS_H\n')

    # Write wrappers implementation
    with open('build/generated/libc-wrappers.c', 'w') as f:
        f.write('// Auto-generated libc passthrough wrappers\n')
        f.write('// Generated by tools/generate-libc-calls.py\n')
        f.write('#define _GNU_SOURCE\n')
        f.write('#define __STDC_WANT_IEC_60559_TYPES_EXT__\n')
        f.write('#include "libc-wrappers.h"\n')
        f.write('#include "wasm3.h"\n')
        f.write('#include "m3_env.h"\n\n')
        # Base headers that must come first
        f.write('#include <stddef.h>\n')
        f.write('#include <stdint.h>\n')
        f.write('#include <sys/types.h>\n')
        f.write('#include <sys/ptrace.h>\n\n')
        # Include only headers that are actually used (filter out internal bits/* headers)
        for header in sorted(used_headers):
            if not header or header in ('stddef.h', 'stdint.h', 'sys/types.h', 'unknown', ''):
                continue
            # Skip internal implementation headers
            if '/bits/' in header or header.startswith('bits/'):
                continue
            if 'x86_64-linux-gnu' in header:
                continue
            f.write(f'#include <{header}>\n')
        f.write('\n')

        # Generate varargs call infrastructure - generic for ALL varargs functions
        f.write('''// ============================================================
// GENERATED: Generic varargs trampoline
// WASM varargs: func(fixed_args..., va_ptr) where va_ptr -> packed args
// Calls the SAME native function (printf calls printf, not vprintf)
// ============================================================

// Assembly trampoline: call any function with args from array
// void* call_native_varargs(void* func, void* args, int arg_count);
extern void* call_native_varargs(void* func, void* args, int arg_count);

''')

        for wrapper in wrappers:
            f.write(wrapper)
            f.write('\n\n')

        f.write('void linkLibcFunctions(IM3Module module) {\n')
        for call in link_calls:
            f.write(call)
            f.write('\n')
        f.write('}\n')

    print(f"Generated {len(wrappers)} wrappers in build/generated/libc-wrappers.c")
    total_skipped = sum(len(v) for v in skipped.values())
    print(f"Skipped {total_skipped} functions:")
    for reason, names in sorted(skipped.items()):
        print(f"  - {reason}: {len(names)}")

    # Generate smoke tests only for functions in glibc exports
    test_count = 0
    for func in functions:
        if func['name'] not in glibc_exports:
            continue
        test = generate_test(func)
        if test:
            test_path = f'build/generated/smoke-tests/test_{func["name"]}.c'
            with open(test_path, 'w') as f:
                f.write(test)
            test_count += 1

    print(f"Generated {test_count} smoke tests in build/generated/smoke-tests/")

    # Write skipped functions list
    with open('build/generated/skipped-functions.txt', 'w') as f:
        f.write('# Skipped functions by reason\n\n')
        for reason, names in sorted(skipped.items()):
            f.write(f'# {reason} ({len(names)} functions)\n')
            for name in sorted(names):
                f.write(f'{name}\n')
            f.write('\n')

    print(f"Skipped functions list: build/generated/skipped-functions.txt")

if __name__ == '__main__':
    main()
