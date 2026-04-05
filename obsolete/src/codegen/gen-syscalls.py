#!/usr/bin/env python3
"""
YOS Syscall Code Generator (Pure C)

Reads syscalls.yaml and generates:
  1. wasm-stubs/yos-generated.c   - WASM-side stubs
  2. wasm-stubs/yos-generated.h   - Header for wasm side
  3. src/yos-handlers-generated.h - Native-side m3Api handlers (pure C)
  4. src/yos-link-generated.h     - Link table for wasm3 (pure C)

Handler types:
  - passthrough: direct native call
  - yos: yos_*() function via yos-runtime.h
  - stub: return fixed value
"""

import yaml
import sys
from pathlib import Path
from datetime import datetime

WASM_TYPES = {
    'i32': 'int32_t',
    'i64': 'int64_t',
    'u32': 'uint32_t',
    'u64': 'uint64_t',
    'f32': 'float',
    'f64': 'double',
    'str': 'const char*',
    'ptr': 'void*',
    'handle': 'void*',  # opaque handle - NOT converted to memory pointer
    'void': 'void',
}

M3_SIG = {
    'i32': 'i', 'i64': 'I', 'u32': 'i', 'u64': 'I',
    'f32': 'f', 'f64': 'F',
    'str': '*', 'ptr': '*', 'handle': 'i', 'void': 'v',  # handle uses 'i' (raw int)
}

def parse_type(t):
    if t.startswith('out:'): return (t[4:], True)
    return (t, False)

def c_type(t):
    base, _ = parse_type(t)
    return WASM_TYPES.get(base, 'void*')

def m3_sig(t):
    base, _ = parse_type(t)
    return M3_SIG.get(base, '*')

def parse_params(params):
    result = []
    for p in params:
        if isinstance(p, dict):
            for pname, ptype in p.items():
                result.append((pname, ptype))
        else:
            pname, ptype = p.split(': ')
            result.append((pname, ptype))
    return result

def m3_getter(ptype, pname):
    base, _ = parse_type(ptype)
    ctype = c_type(ptype)
    # str/ptr get converted to memory pointers; handle is passed as raw value
    if base in ('str', 'ptr') or base not in WASM_TYPES:
        return f'm3ApiGetArgMem({ctype}, {pname})'
    # handle type: read as uint32 then cast to void* (NOT memory-converted)
    if base == 'handle':
        return f'm3ApiGetArg(uint32_t, _{pname}_raw); void* {pname} = (void*)(uintptr_t)_{pname}_raw'
    return f'm3ApiGetArg({ctype}, {pname})'

# Generate WASM wrapper for variadic function
def generate_variadic_wasm_wrapper(func, func_id):
    name = func['name']
    params = parse_params(func.get('params', []))
    ret_type = c_type(func.get('returns', 'void'))

    # Build parameter list with ... at end
    param_strs = [f'{c_type(ptype)} {pname}' for pname, ptype in params]
    param_strs.append('...')
    param_list = ', '.join(param_strs)

    # Fixed args to pass to bridge
    fixed_args = [pname for pname, _ in params]

    # Generate wrapper that packs varargs
    lines = [f'{ret_type} {name}({param_list}) {{']
    lines.append('    VarArgPack pack = {0};')
    lines.append('    __builtin_va_list ap;')
    lines.append(f'    __builtin_va_start(ap, {fixed_args[-1] if fixed_args else "..."});')

    # Find format param (usually named fmt or format)
    fmt_param = None
    for pname, ptype in params:
        if pname in ('fmt', 'format') and ptype == 'str':
            fmt_param = pname
            break

    if fmt_param:
        lines.append(f'    __varargs_pack_printf({fmt_param}, ap, &pack);')
    else:
        lines.append('    // No format string found, pack nothing')

    lines.append('    __builtin_va_end(ap);')

    # Call bridge with fixed args + pack
    if len(fixed_args) == 0:
        args = 'NULL, NULL, NULL'
    elif len(fixed_args) == 1:
        args = f'(void*){fixed_args[0]}, NULL, NULL'
    elif len(fixed_args) == 2:
        args = f'(void*){fixed_args[0]}, (void*){fixed_args[1]}, NULL'
    else:
        args = f'(void*){fixed_args[0]}, (void*){fixed_args[1]}, (void*){fixed_args[2]}'

    if ret_type == 'void':
        lines.append(f'    __yos_varargs_call({func_id}, {args}, &pack);')
    else:
        lines.append(f'    return __yos_varargs_call({func_id}, {args}, &pack);')

    lines.append('}')
    return '\n'.join(lines)

def generate_wasm_stub(func, namespace):
    name = func['name']
    params = parse_params(func.get('params', []))
    returns = func.get('returns', 'void')
    handler = func.get('handler', 'stub')

    param_strs = [f'{c_type(ptype)} {pname}' for pname, ptype in params]
    param_names = [pname for pname, _ in params]
    param_list = ', '.join(param_strs) if param_strs else 'void'
    call_args = ', '.join(param_names)
    ret_type = c_type(returns)

    # wasm_variadic: handled separately
    if func.get('wasm_variadic'):
        return None, None

    # wasm_valist: va_list functions that forward to varargs_call
    if func.get('wasm_valist'):
        return None, None  # generated separately

    # varargs_call is declared in varargs infrastructure block, skip here
    if name == 'varargs_call':
        return None, None

    # wasm_impl: generate actual implementation, no import
    if handler == 'wasm_impl':
        body = func.get('body', 'return 0;' if returns != 'void' else '')
        impl = f'{ret_type} {name}({param_list}) {{ {body} }}'
        return None, impl

    # Other handlers: generate import declaration and wrapper
    import_name = f'__{namespace}_{name}'
    import_decl = f'extern {ret_type} {import_name}({param_list}) __attribute__((import_module("{namespace}"), import_name("{name}")));'

    if returns == 'void':
        wrapper = f'{ret_type} {name}({param_list}) {{ {import_name}({call_args}); }}'
    else:
        wrapper = f'{ret_type} {name}({param_list}) {{ return {import_name}({call_args}); }}'

    return import_decl, wrapper

def generate_native_handler(func, namespace):
    """Generate pure C m3ApiRawFunction handler using yos_* functions."""
    name = func['name']
    params = parse_params(func.get('params', []))
    returns = func.get('returns', 'void')
    handler = func.get('handler', 'stub')
    noreturn = func.get('noreturn', False)

    # wasm_variadic: handled by varargs_call, no native handler needed
    if func.get('wasm_variadic'):
        return None

    # wasm_valist: handled by varargs_call, no native handler needed
    if func.get('wasm_valist'):
        return None

    # wasm_impl runs in wasm, no native handler needed
    if handler == 'wasm_impl':
        return None

    # extern: handler is provided externally, skip generation
    if handler == 'extern':
        return None

    lines = [f'm3ApiRawFunction(handler_{namespace}_{name}) {{']
    lines.append(f'    YOS_TRACE("syscall {name}");')

    if returns != 'void':
        lines.append(f'    m3ApiReturnType({c_type(returns)});')

    # Get parameters
    for pname, ptype in params:
        lines.append(f'    {m3_getter(ptype, pname)};')

    ret_ctype = c_type(returns)

    # Generate call based on handler type
    if handler == 'stub':
        value = func.get('value', 0)
        if returns == 'void':
            lines.append(f'    YOS_WARN("stub function {name} called");')
            lines.append('    m3ApiSuccess();')
        else:
            lines.append(f'    YOS_WARN("stub function {name} called, returning {value}");')
            lines.append(f'    m3ApiReturn({value});')

    elif handler == 'passthrough':
        native = func.get('native', f'{name}({", ".join(p[0] for p in params)})')
        if returns == 'void':
            lines.append(f'    {native};')
            lines.append('    m3ApiSuccess();')
        elif returns == 'ptr':
            # Pointer returns: convert host pointer back to WASM address
            lines.append(f'    {ret_ctype} _r = {native};')
            lines.append('    if (_r == NULL) m3ApiReturn(0);')
            lines.append('    uint32_t _msz = 0;')
            lines.append('    uint8_t* _mbase = m3_GetMemory(runtime, &_msz, 0);')
            lines.append('    m3ApiReturn((void*)(uintptr_t)((uint8_t*)_r - _mbase));')
        elif returns in ('f32', 'f64'):
            # Float returns: no errno check needed
            lines.append(f'    {ret_ctype} _r = {native};')
            lines.append('    m3ApiReturn(_r);')
        else:
            # Integer returns: check for error via < 0
            lines.append(f'    {ret_ctype} _r = {native};')
            lines.append('    m3ApiReturn(_r < 0 ? -errno : _r);')

    elif handler in ('yos', 'vfs', 'runtime'):
        # Call yos_* function from yos-runtime.h
        # 'vfs' and 'runtime' are legacy names, all use yos_* functions now
        lines.append('    yos_exec_ctx_t* ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);')
        method = func.get('method', f'{name}({", ".join(p[0] for p in params)})')
        # Parse method to extract function name and args
        if '(' in method:
            method_name, method_args = method.split('(', 1)
            method_args = method_args.rstrip(')')
        else:
            method_name = method
            method_args = ', '.join(p[0] for p in params)
        # Prepend yos_ and add ctx
        if method_args:
            call = f'yos_{method_name}(ctx, {method_args})'
        else:
            call = f'yos_{method_name}(ctx)'
        if noreturn:
            lines.append(f'    {call};')
            lines.append(f'    m3ApiTrap("{name}");')
        elif returns == 'void':
            lines.append(f'    {call};')
            lines.append('    m3ApiSuccess();')
        else:
            lines.append(f'    {ret_ctype} _r = {call};')
            lines.append('    m3ApiReturn(_r);')

    lines.append('}')
    return '\n'.join(lines)

def generate_link_entry(func, namespace):
    handler = func.get('handler', 'stub')
    # wasm_variadic: handled by varargs_call, no link entry needed
    if func.get('wasm_variadic'):
        return None
    # wasm_valist: handled by varargs_call, no link entry needed
    if func.get('wasm_valist'):
        return None
    # wasm_impl runs in wasm, no link entry needed
    if handler == 'wasm_impl':
        return None

    name = func['name']
    params = parse_params(func.get('params', []))
    returns = func.get('returns', 'void')

    ret_sig = m3_sig(returns)
    param_sig = ''.join(m3_sig(ptype) for _, ptype in params)
    sig = f'{ret_sig}({param_sig})'

    return f'm3_LinkRawFunction(module, "{namespace}", "{name}", "{sig}", handler_{namespace}_{name});'

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <syscalls.yaml> <output_dir>")
        sys.exit(1)

    yaml_path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])

    with open(yaml_path) as f:
        config = yaml.safe_load(f)

    default_namespace = config.get('namespace', 'yos')

    # Collect functions with their namespace
    # Categories prefixed with 'passthrough_' use 'passthrough' namespace
    all_funcs = []  # list of (func, namespace)
    for category, funcs in config.items():
        if category in ('namespace', 'structs'):
            continue
        if isinstance(funcs, list):
            ns = 'passthrough' if category.startswith('passthrough_') else default_namespace
            for func in funcs:
                all_funcs.append((func, ns))

    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    header = f'''// AUTO-GENERATED - DO NOT EDIT
// Generated: {timestamp}
'''

    # Separate variadic and non-variadic functions
    variadic_funcs = [(f, ns) for f, ns in all_funcs if f.get('wasm_variadic')]
    regular_funcs = [(f, ns) for f, ns in all_funcs if not f.get('wasm_variadic')]

    # WASM header
    wasm_h = [header, '#pragma once', '#include <stdint.h>', '']
    for func, ns in regular_funcs:
        name = func['name']
        params = parse_params(func.get('params', []))
        returns = func.get('returns', 'void')
        param_list = ', '.join(f'{c_type(ptype)} {pname}' for pname, ptype in params) or 'void'
        wasm_h.append(f'{c_type(returns)} {name}({param_list});')

    (output_dir / 'wasm-stubs').mkdir(parents=True, exist_ok=True)
    (output_dir / 'wasm-stubs' / 'yos-generated.h').write_text('\n'.join(wasm_h) + '\n')

    # WASM stubs
    wasm_c = [header, '#include "yos-generated.h"', '#include <stddef.h>', '''
// dirent struct for readdir
struct dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};
''']

    # Generate varargs infrastructure if any variadic functions
    if variadic_funcs:
        wasm_c.append('''
// Varargs bridge infrastructure
#define VARG_END    0
#define VARG_INT    1
#define VARG_LONG   2
#define VARG_STR    3
#define VARG_PTR    4
#define VARG_UINT   5
#define VARG_ULONG  6
#define VARG_CHAR   7
#define VARG_MAX    16

typedef struct { unsigned char types[VARG_MAX]; unsigned long values[VARG_MAX]; } VarArgPack;

extern int __yos_varargs_call(int func_id, void* a1, void* a2, void* a3, VarArgPack* pack)
    __attribute__((import_module("yos"), import_name("varargs_call")));

static void __varargs_pack_printf(const char* fmt, __builtin_va_list ap, VarArgPack* pack) {
    int n = 0;
    const char* p = fmt;
    while (*p && n < VARG_MAX) {
        if (*p != '%') { p++; continue; }
        p++;
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') p++;
        while (*p >= '0' && *p <= '9') p++;
        if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }
        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; if (*p == 'l') { is_long = 2; p++; } }
        else if (*p == 'h') { p++; if (*p == 'h') p++; }
        else if (*p == 'z' || *p == 'j' || *p == 't') { is_long = 1; p++; }
        switch (*p) {
            case 'd': case 'i':
                pack->types[n] = is_long ? VARG_LONG : VARG_INT;
                pack->values[n++] = is_long ? (unsigned long)__builtin_va_arg(ap, long) : (unsigned long)__builtin_va_arg(ap, int);
                break;
            case 'u': case 'x': case 'X': case 'o':
                pack->types[n] = is_long ? VARG_ULONG : VARG_UINT;
                pack->values[n++] = is_long ? __builtin_va_arg(ap, unsigned long) : (unsigned long)__builtin_va_arg(ap, unsigned int);
                break;
            case 's':
                pack->types[n] = VARG_STR;
                pack->values[n++] = (unsigned long)__builtin_va_arg(ap, const char*);
                break;
            case 'p':
                pack->types[n] = VARG_PTR;
                pack->values[n++] = (unsigned long)__builtin_va_arg(ap, void*);
                break;
            case 'c':
                pack->types[n] = VARG_CHAR;
                pack->values[n++] = (unsigned long)__builtin_va_arg(ap, int);
                break;
            case '%': break;
        }
        if (*p) p++;
    }
    pack->types[n] = VARG_END;
}
''')
        # Generate function ID defines
        for idx, (func, ns) in enumerate(variadic_funcs):
            wasm_c.append(f'#define VFUNC_{func["name"].upper()} {idx + 1}')
        wasm_c.append('')

        # Generate wrappers for each variadic function
        for idx, (func, ns) in enumerate(variadic_funcs):
            func_id = idx + 1
            wasm_c.append(generate_variadic_wasm_wrapper(func, func_id))
            wasm_c.append('')

    # Generate wasm_valist wrappers (va_list functions that forward to varargs_call)
    valist_funcs = [(f, ns) for f, ns in all_funcs if f.get('wasm_valist')]
    for func, ns in valist_funcs:
        name = func['name']
        params = parse_params(func.get('params', []))
        ret_type = c_type(func.get('returns', 'void'))
        vfunc_id = func.get('vfunc_id', 0)

        # Build param list - last param is ap (va_list)
        param_strs = [f'{c_type(ptype)} {pname}' for pname, ptype in params]
        param_list = ', '.join(param_strs)

        # Build args for varargs_call: func_id, arg1, arg2, arg3, pack
        # ap is the last param and becomes pack, others become arg1/arg2/arg3
        fixed_params = [(pname, ptype) for pname, ptype in params if pname != 'ap']
        args = ['NULL', 'NULL', 'NULL']
        for i, (pname, ptype) in enumerate(fixed_params[:3]):
            args[i] = f'(void*){pname}'

        wasm_c.append(f'{ret_type} {name}({param_list}) {{')
        wasm_c.append(f'    return __yos_varargs_call({vfunc_id}, {args[0]}, {args[1]}, {args[2]}, ap);')
        wasm_c.append('}')
        wasm_c.append('')

    imports, wrappers = [], []
    for func, ns in all_funcs:
        imp, wrap = generate_wasm_stub(func, ns)
        if imp:  # None for wasm_impl or wasm_variadic
            imports.append(imp)
        if wrap:  # None for wasm_variadic
            wrappers.append(wrap)
    wasm_c.extend(imports)
    wasm_c.append('')
    wasm_c.extend(wrappers)
    (output_dir / 'wasm-stubs' / 'yos-generated.c').write_text('\n'.join(wasm_c) + '\n')

    # Native handlers - PURE C
    native = [header, '''#ifndef YOS_HANDLERS_GENERATED_H
#define YOS_HANDLERS_GENERATED_H

#include "wasm3.h"
#include "m3_env.h"
#include "yos-types.h"
#include "yos-vfs.h"
#include "yos-process.h"
#include "yos-runtime.h"
#include "yos-log.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

// Debug macro for syscall tracing
#define YOS_DBG(fmt, ...) YOS_TRACE(fmt, ##__VA_ARGS__)

''']
    # Add forward declarations for extern handlers
    for func, ns in all_funcs:
        if func.get('handler') == 'extern':
            native.append(f'm3ApiRawFunction(handler_{ns}_{func["name"]});')
    native.append('')

    for func, ns in all_funcs:
        handler = generate_native_handler(func, ns)
        if handler:  # None for wasm_impl
            native.append(handler)
            native.append('')

    native.append('#endif // YOS_HANDLERS_GENERATED_H')

    (output_dir / 'src').mkdir(parents=True, exist_ok=True)
    (output_dir / 'src' / 'yos-handlers-generated.h').write_text('\n'.join(native) + '\n')

    # Link table - PURE C
    link = [header, '''#ifndef YOS_LINK_GENERATED_H
#define YOS_LINK_GENERATED_H

#include "wasm3.h"
#include "yos-handlers-generated.h"

static inline void link_yos_functions(IM3Module module) {
''']
    for func, ns in all_funcs:
        entry = generate_link_entry(func, ns)
        if entry:  # None for wasm_impl
            link.append('    ' + entry)
    link.append('}')
    link.append('')
    link.append('#endif // YOS_LINK_GENERATED_H')
    (output_dir / 'src' / 'yos-link-generated.h').write_text('\n'.join(link) + '\n')

    print(f"Generated {len(all_funcs)} syscalls to {output_dir}")

if __name__ == '__main__':
    main()
