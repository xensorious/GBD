"""Replay a sanitized capture/Pokedex allocation trace through the ROM allocator.

The former fragmented layout asserted on the final 8192-byte sprite request.
This checks heap capacity and payload isolation; it does not emulate the UI.
"""
import json
from pathlib import Path
import struct

from target import ROM, PARTS
from tools.code_only_relocate import clone_parts
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
)


HEAP = 0x02000000
HEAP_SIZE = 0x1c000
END = 0x03007c00
STACK = 0x03006f00
FIXTURE = Path(__file__).with_name('pokedex_allocations.json')


class TargetHeap:
    def __init__(self):
        self.u = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
        for address, size in [(0x08000000, 0x02000000), (HEAP, 0x40000),
                              (0x03000000, 0x8000), (0x04000000, 0x1000)]:
            self.u.mem_map(address, size)
        self.u.mem_write(0x08000000, ROM)
        callback_data = next(data for name, _, data in PARTS if name == '.callbacks')
        battle_guard = struct.unpack_from('<I', callback_data)[0]
        self.parts = PARTS + clone_parts(ROM, battle_guard)
        for _, address, data in self.parts:
            self.u.mem_write(address, data)
        self.u.mem_write(0x080005ec, struct.pack('<I', 0x0200cecd))
        self.u.mem_write(0x08002b84, struct.pack('<H', 0x4710))
        self.u.mem_write(0x08002b96, struct.pack('<H', 0x0201))
        self.assertion = None
        self.u.hook_add(UC_HOOK_CODE, self._hook)
        self.call(0x08002b81, HEAP, HEAP_SIZE)
        for name, address, data in self.parts:
            assert bytes(self.u.mem_read(address, len(data))) == data, name
        self.protected = [(address, size) for address, used, size in self.blocks() if used]
        self.before = [bytes(self.u.mem_read(address + 16, size))
                       for address, size in self.protected]

    def _hook(self, u, address, size, _):
        if address == END:
            u.emu_stop()
        elif address == 0x081e3b14:
            # Stop before the ROM's allocation assertion aborts execution.
            self.assertion = u.reg_read(UC_ARM_REG_R1)
            u.emu_stop()

    def call(self, entry, *args):
        self.assertion = None
        self.u.reg_write(UC_ARM_REG_SP, STACK)
        self.u.reg_write(UC_ARM_REG_LR, END | 1)
        for register, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1,
                                    UC_ARM_REG_R2, UC_ARM_REG_R3), args):
            self.u.reg_write(register, value)
        self.u.emu_start(entry, 0, count=100000)
        assert self.assertion is None, f'ROM heap assertion, malloc.c line {self.assertion}'
        assert self.u.reg_read(UC_ARM_REG_PC) == END, 'ROM call exceeded instruction budget'
        assert self.u.reg_read(UC_ARM_REG_SP) == STACK, 'ROM call unbalanced the stack'
        return self.u.reg_read(UC_ARM_REG_R0)

    def blocks(self):
        address = HEAP
        previous = HEAP
        seen = set()
        while address not in seen:
            seen.add(address)
            used, magic, size, prev, nxt = struct.unpack(
                '<HHIII', self.u.mem_read(address, 16))
            assert HEAP <= address < HEAP + HEAP_SIZE and magic == 0xa3a3
            assert used in (0, 1) and prev == previous
            assert address + 16 + size <= HEAP + HEAP_SIZE
            yield address, used, size
            if nxt == HEAP:
                assert address + 16 + size == HEAP + HEAP_SIZE
                return
            assert nxt == address + 16 + size
            previous, address = address, nxt
        raise AssertionError('heap chain loops before its end')

    def preserve_payload(self):
        for (address, size), before in zip(self.protected, self.before):
            assert bytes(self.u.mem_read(address + 16, size)) == before, (
                f'allocated buffer overwrote protected payload at {address + 16:08x}')
        list(self.blocks())

    def allocate(self, identifier, size):
        pointer = self.call(0x08002b9d, size)
        assert pointer, f'allocation {identifier}, size {size}, returned NULL'
        used, magic, allocated, _, _ = struct.unpack(
            '<HHIII', self.u.mem_read(pointer - 16, 16))
        assert used == 1 and magic == 0xa3a3 and allocated >= size
        assert HEAP + 16 <= pointer and pointer + allocated <= HEAP + HEAP_SIZE
        assert not any(pointer < address + 16 + length and pointer + allocated > address
                       for address, length in self.protected)
        self.u.mem_write(pointer, bytes([(identifier * 37 + 11) & 255]) * allocated)
        self.preserve_payload()
        return pointer

    def free(self, pointer):
        self.call(0x08002bc5, pointer)
        self.preserve_payload()


def main():
    fixture = json.loads(FIXTURE.read_text())
    assert fixture['schema_version'] == 1
    assert sum(event['op'] == 'alloc' for event in fixture['events']) == 169
    target = TargetHeap()
    active = {}
    for initial in fixture['initial_allocations']:
        pointer = target.allocate(initial['id'], initial['size'])
        assert pointer == int(initial['expected_pointer'], 16)
        active[initial['id']] = pointer

    requests = 0
    for index, event in enumerate(fixture['events']):
        identifier = event['id']
        try:
            if event['op'] == 'alloc':
                assert identifier not in active
                active[identifier] = target.allocate(identifier, event['size'])
                requests += 1
            else:
                assert event['op'] == 'free'
                target.free(active.pop(identifier))
        except AssertionError as error:
            raise AssertionError(f'Pokedex trace event {index}, {event}: {error}') from error

    # All temporary buffers remain independently freeable after the replay.
    for pointer in active.values():
        target.free(pointer)
    assert [(address, size) for address, used, size in target.blocks() if used] == target.protected
    print(f'PASS {requests} capture/Pokedex allocation requests through ROM Alloc/Free; '
          'protected code, clone, tables and state survive buffer fills and frees')


if __name__ == '__main__':
    main()
