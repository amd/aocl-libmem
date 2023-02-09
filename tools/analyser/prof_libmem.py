#!/usr/bin/env python3
#
# Copywrite (C) 2022 Advanced Micro Devices, Inc. All rights reserved.


from bcc import BPF
from bcc.libbcc import lib, bcc_symbol, bcc_symbol_option

import ctypes as ct
import sys, os
import time
import argparse
import getpass
import logging
LOG = logging.getLogger(__name__)

# Work around a bug in older versions of bcc.libbcc
print(bcc_symbol_option.__dict__)
if len(bcc_symbol_option._fields_) == 3:
    LOG.warning("Adding workaround for old libBCC")
    class patched_bcc_symbol_option(ct.Structure):
        _fields_ = [
            ('use_debug_file', ct.c_int),
            ('check_debug_file_crc', ct.c_int),
            ('lazy_symbolize', ct.c_int),
            ('use_symbol_type', ct.c_uint),
        ]
    my_bcc_symbol_option = patched_bcc_symbol_option
else:
    my_bcc_symbol_option = bcc_symbol_option

class FuncInfo():
    STT_GNU_IFUNC = 1 << 10
    def __init__(self, libname, name, symbol, tgtArg):
        self.libname = libname
        self.name = name
        self.symbol = symbol
        self.argNum = tgtArg
        self.is_indirect = False
        self.indirect_symbol = None

    def attach_point(self):
        """ Returns a tuple to compare if multiple FuncInfo would attach to the same point """
        return (self.libname, self.indirect_func_offset if self.is_indirect else self.symbol)

    def resolve_symbol(self):
        new_symbol = self._get_indirect_function_sym(self.libname, self.symbol)
        if not new_symbol:
            LOG.debug('%s is not an indirect function', self.name);
            self.is_indirect = False
        else:
            LOG.debug('%s IS an indirect function', self.name);
            self.is_indirect = True
            self.indirect_symbol = new_symbol
            self._find_impl_func_offset()

    def attach(self, b, pid, fn_name):
        if self.is_indirect:
            b.attach_uprobe(name=ct.cast(self.indirect_symbol.module, ct.c_char_p).value,
                       addr=self.indirect_func_offset, fn_name=fn_name, pid=pid)
        else:
            b.attach_uprobe(name=self.libname, sym=self.symbol, fn_name=fn_name, pid=pid)


    def _get_indirect_function_sym(self, module, symname):
        sym = bcc_symbol()
        sym_op = my_bcc_symbol_option()
        sym_op.use_debug_file = 1
        sym_op.check_debug_file_crc = 1
        sym_op.lazy_symbolize = 1
        sym_op.use_symbol_type = FuncInfo.STT_GNU_IFUNC
        ct.set_errno(0)
        retval = lib.bcc_resolve_symname(
                module.encode(),
                symname.encode(),
                0x0,
                0,
                ct.cast(ct.byref(sym_op), ct.POINTER(bcc_symbol_option)),
                ct.byref(sym),
        )
        LOG.debug('Got sym name: %s, offset: 0x%x', sym.name, sym.offset)
        LOG.debug("Lookup for func %s returned %d", symname, retval)
        if retval < 0:
            LOG.debug("ERRNO is %d\n", ct.get_errno())
            return None
        else:
            return sym

    def _find_impl_func_offset(self):
        resolv_func_addr = None
        impl_func_addr = None
        SUBMIT_FUNC_ADDR_BPF_TEXT = """
#include <uapi/linux/ptrace.h>

BPF_PERF_OUTPUT(impl_func_addr);
void submit_impl_func_addr(struct pt_regs *ctx) {
    u64 addr = PT_REGS_RC(ctx);
    impl_func_addr.perf_submit(ctx, &addr, sizeof(addr));
}


BPF_PERF_OUTPUT(resolv_func_addr);
int submit_resolv_func_addr(struct pt_regs *ctx) {
    u64 rip = PT_REGS_IP(ctx);
    resolv_func_addr.perf_submit(ctx, &rip, sizeof(rip));
    return 0;
}
"""

        def set_impl_func_addr(cpu, data, size):
            addr = ct.cast(data, ct.POINTER(ct.c_uint64)).contents.value
            nonlocal impl_func_addr
            impl_func_addr = addr


        def set_resolv_func_addr(cpu, data, size):
            addr = ct.cast(data, ct.POINTER(ct.c_uint64)).contents.value
            nonlocal resolv_func_addr
            resolv_func_addr = addr


        b = BPF(text=SUBMIT_FUNC_ADDR_BPF_TEXT)
        b.attach_uprobe(name=self.libname, addr=self.indirect_symbol.offset, fn_name=b'submit_resolv_func_addr', pid=os.getpid())
        b['resolv_func_addr'].open_perf_buffer(set_resolv_func_addr)
        b.attach_uretprobe(name=self.libname, addr=self.indirect_symbol.offset, fn_name=b"submit_impl_func_addr", pid=os.getpid())
        b['impl_func_addr'].open_perf_buffer(set_impl_func_addr)
    
        LOG.debug('wait for the first %s call', self.name)
        libc = ct.CDLL("libc.so.6")
        while True:
            try:
                if resolv_func_addr and impl_func_addr:
                    b.detach_uprobe(name=self.libname, addr=self.indirect_symbol.offset, pid=os.getpid())
                    b.detach_uretprobe(name=self.libname, addr=self.indirect_symbol.offset, pid=os.getpid())
                    b.cleanup()
                    break
# force an invocation of our targeted function, so our probes fire.
# TODO Note: this only works for our few functions in libc that all share
# the same signature of (void*, void*, size_t).
# We would need more involved arg management for more generic functions
                libc.__getattr__(self.name)(None, None, 0);
    
                b.perf_buffer_poll()
            except KeyboardInterrupt:
                exit()
        LOG.debug('IFUNC resolution of %s is performed', self.name)
        LOG.debug('\tresolver function address      : 0x%x', resolv_func_addr)
        LOG.debug('\tresolver function offset       : 0x%x', self.indirect_symbol.offset)
        LOG.debug('\tfunction implementation address: 0x%x', impl_func_addr)
        impl_func_offset = impl_func_addr - resolv_func_addr + self.indirect_symbol.offset
        LOG.debug('\tfunction implementation offset : 0x%x', impl_func_offset)
        self.indirect_func_offset = impl_func_offset





def _build_bpf_text(functions):
    text = """
#include <uapi/linux/ptrace.h>
"""

    for func in functions:
        text += """

BPF_HISTOGRAM(lenHist_{0});
int count_{0}(struct pt_regs *ctx) {{
    size_t len = PT_REGS_PARM{1}(ctx);
    lenHist_{0}.increment(bpf_log2l(len));
    return 0;
}}
""".format(func.name,func.argNum)
    return text


def dedup_functions(all_funcs):
    keys = dict()
    for func in all_funcs:
        if func.attach_point() in keys.keys():
            LOG.warning("%s is a duplicate target to %s. Skipping tracing.", func.name, keys.get(func.attach_point()).name)
        else:
            keys[func.attach_point()] = func

    return keys.values()


def main():
    TARGET_FUNCTIONS = [
        FuncInfo('c', 'memcpy',  'memcpy@@GLIBC_2.14', 3),
        FuncInfo('c', 'mempcpy', 'mempcpy', 3),
        FuncInfo('c', 'memcmp',  'memcmp', 3),
        FuncInfo('c', 'memmove', 'memmove', 3),
        FuncInfo('c', 'memset',  'memset', 3),
    ]

    p = argparse.ArgumentParser(
        prog = 'prof_libmem',
        description = 'Trace all calls to aocl-libmem replacable functions')
    p.add_argument('-i', '--interval', default=5, type=int,
        help = 'How often (in seconds) to report data')
    p.add_argument('-p', '--pid', type=int, default=-1,
        help = 'Trace only this PID, or -1 to trace entire system')
    p.add_argument('-f', '--functions', action='append',
        choices=[x.name for x in TARGET_FUNCTIONS],
        help = 'Trace only functions listed')
    p.add_argument('-v', '--verbose', action='count', default=0)

    args = p.parse_args()

    if args.verbose >= 2:
        LOG.setLevel(logging.DEBUG)
    elif args.verbose:
        LOG.setLevel(logging.INFO)
    LOG.addHandler(logging.StreamHandler())

    if args.functions:
        all_funcs = [x for x in TARGET_FUNCTIONS if x.name in args.functions]
    else:
        all_funcs = TARGET_FUNCTIONS

    # Check if user is root
    if getpass.getuser() != 'root':
        LOG.error("This application must be run with superuser privledges")
        return

    LOG.info("Starting Symbol resolution")
    for func in all_funcs:
        func.resolve_symbol()
    
    unique_funcs = dedup_functions(all_funcs)


    LOG.info("Attaching BPF tracers")
    b = BPF(text=_build_bpf_text(unique_funcs))
    for funcInfo in unique_funcs:
        fn_name='count_{}'.format(funcInfo.name).encode()
        funcInfo.attach(b, args.pid, fn_name)
        funcInfo.histo = b['lenHist_{}'.format(funcInfo.name)]

    LOG.info("Beginning tracing")
    try:
        while True:
            time.sleep(args.interval)
            print('%-8s\n' % time.strftime('%H:%M:%S'), end='')
            for func in unique_funcs:
                func.histo.print_log2_hist(func.name + ' length:')
                func.histo.clear()

    except KeyboardInterrupt:
        pass

main()
