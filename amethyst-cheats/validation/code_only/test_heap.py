"""Target-backed checks for sparse heap reservation and InitHeap interworking."""
import struct
from target import ROM, PARTS
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *

ROOT = 0x02000000
HEAP_SIZE = 0x1c000
S = 0x02010c00
END = 0x03007c00
MAGIC = 0x414d5954
RESERVED = [(ROOT + low, ROOT + high) for low, high in [
    (0xb950, 0xbd60), (0xcebc, 0xced4), (0xd0f0, 0xeaf0),
    (0xeff0, 0xf200), (0x10028, 0x10070), (0x10a28, 0x10a48),
    (0x10af0, 0x10b28), (0x10bf0, 0x10e00),
]]
FREE = []
current = ROOT
for low, high in RESERVED:
    FREE.append((current, low))
    current = high
FREE.append((current, ROOT + HEAP_SIZE))
BLOCKS = sorted([(low, high, 1) for low, high in RESERVED] +
                [(low, high, 0) for low, high in FREE])
CALLEE = [UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
          UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11]


def setup():
    u = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    for address, size in [(0x08000000, 0x02000000), (ROOT, 0x40000),
                          (0x03000000, 0x8000), (0x04000000, 0x1000)]:
        u.mem_map(address, size)
    u.mem_write(0x08000000, ROM)
    for name, address, data in PARTS:
        u.mem_write(address, data)
    assert ROM[0x2b84:0x2b86] == bytes.fromhex('1060')
    assert ROM[0x2b96:0x2b98] == bytes.fromhex('0003')
    u.mem_write(0x080005ec, struct.pack('<I', 0x0200cecd))
    u.mem_write(0x08002b84, struct.pack('<H', 0x4710))
    u.mem_write(0x08002b96, struct.pack('<H', 0x0201))
    return u


def w32(u, address, value):
    u.mem_write(address, struct.pack('<I', value))


def r32(u, address):
    return struct.unpack('<I', u.mem_read(address, 4))[0]


def header(u, address):
    return struct.unpack('<HHIII', u.mem_read(address, 16))


def launch(u, entry, args=(), stubs=(), trace=None):
    u.reg_write(UC_ARM_REG_SP, 0x03006f00)
    u.reg_write(UC_ARM_REG_LR, END | 1)
    for index, register in enumerate(CALLEE):
        u.reg_write(register, 0x56780000 + index * 4)
    for register, value in zip([UC_ARM_REG_R0, UC_ARM_REG_R1,
                                UC_ARM_REG_R2, UC_ARM_REG_R3], args):
        u.reg_write(register, value)

    def hook(uc, address, size, _):
        if trace is not None:
            trace.add(address)
        if address == END:
            uc.emu_stop()
        elif address == 0x081e3b14 or address in stubs:
            if address == 0x0806e6d0:
                uc.reg_write(UC_ARM_REG_R0, 0)
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    handle = u.hook_add(UC_HOOK_CODE, hook)
    u.emu_start(entry, 0, count=100000)
    u.hook_del(handle)
    assert u.reg_read(UC_ARM_REG_PC) == END, hex(u.reg_read(UC_ARM_REG_PC))
    assert u.reg_read(UC_ARM_REG_SP) == 0x03006f00
    assert [u.reg_read(register) for register in CALLEE] == [
        0x56780000 + index * 4 for index in range(8)]
    return u.reg_read(UC_ARM_REG_R0)


def check_chain(u):
    for index, (low, high, used) in enumerate(BLOCKS):
        previous = BLOCKS[index - 1][0] if index else ROOT
        nxt = BLOCKS[index + 1][0] if index + 1 < len(BLOCKS) else ROOT
        assert header(u, low) == (used, 0xa3a3, high - low - 16, previous, nxt)


def reserved_data(u):
    return [bytes(u.mem_read(low + 16, high - low - 16))
            for low, high in RESERVED]


# Execute the actual ROM entry, two patched halfwords, and ARM/Thumb trampoline.
# Installed flags and the rest of the 512-byte state survive ordinary resets.
for warm in [False, True]:
    u = setup()
    initial = (struct.pack('<I', MAGIC) + bytes([0xa5]) * 508
               if warm else bytes([0xa5]) * 512)
    u.mem_write(S, initial)
    before_parts = {a: bytes(u.mem_read(a, len(b))) for _, a, b in PARTS}
    launch(u, 0x08002b81, [ROOT, HEAP_SIZE])
    assert r32(u, 0x03000a38) == ROOT
    assert r32(u, 0x03000a3c) == HEAP_SIZE
    expected = initial if warm else struct.pack('<I', MAGIC) + bytes(508)
    check_chain(u)
    assert bytes(u.mem_read(S, 512)) == expected
    for address, data in before_parts.items():
        assert bytes(u.mem_read(address, len(data))) == data
    for _ in range(3):
        launch(u, 0x08002b9d, [0x100])
        launch(u, 0x08002b81, [ROOT, HEAP_SIZE])
        check_chain(u)
        assert bytes(u.mem_read(S, 512)) == expected
print('PASS 2 cold/warm heap cases and 6 repeated resets preserve state')

for start, size in [(0x02008000, 0x1000), (0x02020000, 0x400),
                    (ROOT, 0x1b000)]:
    u = setup()
    state = struct.pack('<I', MAGIC) + bytes([0x7d]) * 508
    u.mem_write(S, state)
    trace = set()
    launch(u, 0x08002b81, [start, size], trace=trace)
    assert 0x08002948 in trace
    assert header(u, start) == (0, 0xa3a3, size - 16, start, start)
    assert r32(u, 0x03000a38) == start and r32(u, 0x03000a3c) == size
    assert bytes(u.mem_read(S, 512)) == state
print('PASS 3 other heap arguments retain the original ROM initializer')

# Exhaust every free interval through the real ROM allocator and fill all
# returned buffers. Every gap must be usable and every reserved payload intact.
u = setup()
launch(u, 0x08002b81, [ROOT, HEAP_SIZE])
before = reserved_data(u)
for low, high in FREE:
    size = high - low - 16
    pointer = launch(u, 0x08002b9d, [size])
    assert pointer == low + 16, (hex(pointer), hex(low + 16))
    u.mem_write(pointer, bytes([0xd3]) * size)
    assert reserved_data(u) == before
assert launch(u, 0x08002b9d, [4]) == 0
assert reserved_data(u) == before
print('PASS all 9 free intervals are usable; 8 reservations survive exhaustion')

# Splits and frees update neighboring headers, but must never use their data.
for requests in [[4] * 80, [1, 17, 0x40, 0x8000, 0x101, 0x3000, 0x2000, 7],
                 [0xb941, 0x1140, 0xb1f0, 0x10, 0x100, 0x7ff, 0x5f0]]:
    u = setup()
    launch(u, 0x08002b81, [ROOT, HEAP_SIZE])
    before = reserved_data(u)
    pointers = []
    for request in requests:
        pointer = launch(u, 0x08002b9d, [request])
        if not pointer:
            continue
        allocated = r32(u, pointer - 12)
        assert allocated >= request
        assert not any(pointer < high and pointer + allocated > low
                       for low, high in RESERVED)
        u.mem_write(pointer, bytes([0x6c]) * allocated)
        pointers.append(pointer)
        assert reserved_data(u) == before
    for pointer in pointers[::2] + pointers[1::2]:
        launch(u, 0x08002bc5, [pointer])
        assert reserved_data(u) == before
    check_chain(u)
print('PASS 3 allocator split/coalesce patterns preserve reserved payloads')

# Refresh cannot reinitialize a fragmented heap or change the heap globals.
u = setup()
launch(u, 0x08002b81, [ROOT, HEAP_SIZE])
for size in [17, 0x800, 0x1000]:
    launch(u, 0x08002b9d, [size])
before = bytes(u.mem_read(ROOT, 0x40000))
globals_before = bytes(u.mem_read(0x03000a38, 8))
trace = set()
for _ in range(5):
    launch(u, 0x08002b81, [0, 0], trace=trace)
assert bytes(u.mem_read(ROOT, 0x40000)) == before
assert bytes(u.mem_read(0x03000a38, 8)) == globals_before
assert 0x08002948 not in trace
print('PASS 5 zero-argument refreshes preserve the live heap and globals')

# There is no ReadKeys bootstrap fallback: a missing magic must leave both
# unreserved heap data and the entire state untouched, even with live ROM hooks.
for marker in [0, 0xa5a5a5a5]:
    u = setup()
    w32(u, S, marker)
    w32(u, 0x03000a38, ROOT)
    w32(u, 0x03000a3c, HEAP_SIZE)
    u.mem_write(ROOT, struct.pack('<HHIII', 0, 0xa3a3, HEAP_SIZE - 16, ROOT, ROOT))
    before = bytes(u.mem_read(ROOT, 0x40000))
    globals_before = bytes(u.mem_read(0x03000a38, 8))
    trace = set()
    launch(u, 0x0200cecd, stubs=[0x0891cecc], trace=trace)
    assert bytes(u.mem_read(ROOT, 0x40000)) == before
    assert bytes(u.mem_read(0x03000a38, 8)) == globals_before
    assert 0x08002b80 not in trace
print('PASS 2 uninstalled ReadKeys cases leave heap, state, and globals untouched')
print('Reserved bytes including 8 headers:', sum(high - low for low, high in RESERVED))
