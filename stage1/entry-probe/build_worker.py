"""Build and QEMU-check a tiny retained-console entry probe inside its unit."""
import base64
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path('/opt/xzpios-lab')
WORK = ROOT / 'stage1-entry-probe-20261027-a'
TOOLS = ROOT / 'lineage20-gcc9-build/toolchain'
HOST = ROOT / 'lineage20-build/toolchain'
env = os.environ.copy()
env['PATH'] = str(TOOLS / 'bin') + ':' + str(HOST / 'usr/bin') + ':/usr/bin:/bin'
env['LD_LIBRARY_PATH'] = ':'.join(str(p) for p in [TOOLS / 'usr/lib/x86_64-linux-gnu',
    HOST / 'usr/lib/x86_64-linux-gnu', HOST / 'lib/x86_64-linux-gnu'])
events = []


def run(args, timeout=12, environment=env):
    p = subprocess.run([str(a) for a in args], cwd=str(WORK), env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, timeout=timeout)
    event = {'args': [str(a) for a in args], 'returncode': p.returncode,
             'stdout': p.stdout, 'stderr': p.stderr}
    events.append(event)
    if p.returncode:
        raise RuntimeError(json.dumps(event))
    return p.stdout


gcc = TOOLS / 'bin/aarch64-linux-gnu-gcc'
ld = TOOLS / 'bin/aarch64-linux-gnu-ld'
objcopy = TOOLS / 'bin/aarch64-linux-gnu-objcopy'
objdump = TOOLS / 'bin/aarch64-linux-gnu-objdump'
compiler = run([gcc, '--version']).splitlines()[0]
linker = run([ld, '--version']).splitlines()[0]
run([gcc, '-nostdlib', '-ffreestanding', '-march=armv8-a', '-c', 'entry.S', '-o', 'entry.o'])
run([ld, '--build-id=none', '-T', 'link.ld', 'entry.o', '-o', 'entry.elf'])
run([objcopy, '-O', 'binary', 'entry.elf', 'Image'])
disassembly = run([objdump, '-d', 'entry.elf'])
symbols = run([TOOLS / 'bin/aarch64-linux-gnu-nm', '-n', 'entry.elf'])
image = (WORK / 'Image').read_bytes()
if len(image) != 4096 or image[56:60] != b'ARMd':
    raise ValueError('Invalid entry Image')
if struct.unpack_from('<QQQ', image, 8) != (0x80000, 4096, 0xa):
    raise ValueError('Unexpected ARM64 Image header')
if image.count(b'XZPIOS_STAGE1_20261027_ENTRY_A\n') != 1:
    raise ValueError('Build marker missing or duplicated')
(WORK / 'entry.disassembly.txt').write_text(disassembly)
(WORK / 'entry.symbols.txt').write_text(symbols)
qemu = ROOT / 'sysroot/usr/bin/qemu-system-aarch64'
if hashlib.sha256(qemu.read_bytes()).hexdigest() != '57d0d6663ff2104db7ca2a41a5b4dea12d0e944dc143d085d88f32692d83f430':
    raise ValueError('QEMU binary changed')
qenv = os.environ.copy()
qenv['LD_LIBRARY_PATH'] = str(ROOT / 'sysroot/lib')
ram = WORK / 'qemu-ram.bin'
with ram.open('wb') as stream:
    stream.truncate(4 * 1024 ** 3)
qcmd = [qemu, '-M', 'virt,gic-version=3,memory-backend=ram', '-cpu', 'cortex-a72',
    '-smp', '1', '-m', '4G', '-accel', 'tcg,thread=single,tb-size=16',
    '-object', 'memory-backend-file,id=ram,size=4G,mem-path=' + str(ram) + ',share=on,prealloc=off',
    '-kernel', str(WORK / 'Image'), '-nodefaults', '-net', 'none', '-display', 'none',
    '-serial', 'none', '-monitor', 'none', '-no-reboot']
p = subprocess.run(['/usr/bin/timeout', '--kill-after=1s', '5s'] + [str(a) for a in qcmd],
    stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=qenv, timeout=8)
with ram.open('rb') as stream:
    stream.seek(0xffcc0000 - 0x40000000)
    captured = stream.read(4096)
sig, start, size = struct.unpack_from('<III', captured)
text = captured[12:12 + size].decode('ascii', 'strict') if 0 <= size <= 4084 else ''
passed = (sig == 0x43474244 and start == size and 100 < size < 1024
    and text.startswith('XZPIOS_STAGE1_20261027_ENTRY_A\n')
    and text.endswith('XZPIOS_STAGE1_END_RESET_SMC\n'))
(WORK / 'qemu-console.bin').write_bytes(captured)
(WORK / 'qemu-console.txt').write_text(text)
allocated = ram.stat().st_blocks * 512
ram.unlink()
result = {'compiler': compiler, 'linker': linker,
    'image_sha256': hashlib.sha256(image).hexdigest(), 'image_size': len(image),
    'source_sha256': hashlib.sha256((WORK / 'entry.S').read_bytes()).hexdigest(),
    'image_base64': base64.b64encode(image).decode('ascii'),
    'disassembly': disassembly, 'symbols': symbols, 'events': events,
    'qemu': {'command': [str(a) for a in qcmd], 'returncode': p.returncode,
        'stderr': p.stderr.decode('utf-8', 'replace'), 'passed': passed,
        'signature': hex(sig), 'start': start, 'size': size, 'console': text,
        'sparse_file_allocated_bytes': allocated,
        'scope': 'Entry code and retained-console encoding; Sony SMC reset is tested on the phone separately.'}}
(WORK / 'result.json').write_text(json.dumps(result, indent=2))
print(json.dumps({'qemu_passed': passed, 'console': text, 'image_sha256': result['image_sha256']}))
if not passed:
    raise SystemExit(1)
