"""Smoke check for the private OpenConnect packet ABI; does not connect a VPN."""
from pathlib import Path
import ctypes
import os
import sys

root = Path(__file__).resolve().parents[1]
engine = Path(sys.argv[1]) if len(sys.argv) > 1 else root / '.tools/openconnect-prefix/bin/libopenconnect-5.dll'
directories = [os.add_dll_directory(str(engine.parent))]
runtime = root / '.tools/msys64/mingw64/bin'
if runtime.is_dir():
    directories.append(os.add_dll_directory(str(runtime)))
lib = ctypes.CDLL(str(engine))
lib.openconnect_init_ssl.restype = ctypes.c_int
assert lib.openconnect_init_ssl() == 0
lib.openconnect_vpninfo_new.argtypes = [ctypes.c_char_p] + [ctypes.c_void_p] * 5
lib.openconnect_vpninfo_new.restype = ctypes.c_void_p
context = lib.openconnect_vpninfo_new(b'Gozarno packet ABI smoke test', None, None, None, None, None)
assert context
callback_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_int)
read_packet = callback_type(lambda context, data, size: 0)
write_packet = callback_type(lambda context, data, size: 0)
lib.openconnect_vpninfo_free.argtypes = [ctypes.c_void_p]
try:
    setup = lib.gozarno_openconnect_packet_io_v1
    setup.argtypes = [ctypes.c_void_p, callback_type, callback_type, ctypes.c_void_p]
    setup.restype = ctypes.c_int
    assert setup(context, read_packet, write_packet, None) == 0
    lib.openconnect_get_ifname.argtypes = [ctypes.c_void_p]
    lib.openconnect_get_ifname.restype = ctypes.c_char_p
    assert lib.openconnect_get_ifname(context) is None
finally:
    lib.openconnect_vpninfo_free(context)
print('PASS: packet ABI initializes and frees without an OS adapter')
