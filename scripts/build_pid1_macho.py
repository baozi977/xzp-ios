"""Run on Aliyun with private GCC/binutils; wrap freestanding code as static Mach-O."""
import base64,hashlib,json,os,struct,subprocess,zlib
from pathlib import Path
ROOT=Path('/opt/xzpios-lab');W=ROOT/'pid1-heartbeat-20261027';W.mkdir(exist_ok=True)
GCC=ROOT/'lineage20-gcc9-build/toolchain';HOST=ROOT/'lineage20-build/toolchain'
env=os.environ.copy();env['LD_LIBRARY_PATH']=':'.join(str(p) for p in [GCC/'usr/lib/x86_64-linux-gnu',HOST/'usr/lib/x86_64-linux-gnu',HOST/'lib/x86_64-linux-gnu'])
env['PATH']=str(GCC/'bin')+':'+str(HOST/'usr/bin')+':/usr/bin:/bin'
commands=[[str(GCC/'bin/aarch64-linux-gnu-gcc'),'-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-fno-pic','-fno-pie','-mgeneral-regs-only','-nostdlib','-static','-no-pie','-Wl,--build-id=none','-Wl,-T,'+str(W/'pid1.ld'),'-o',str(W/'pid1.elf'),str(W/'pid1.c')],
 [str(HOST/'usr/bin/aarch64-linux-gnu-objcopy'),'-O','binary',str(W/'pid1.elf'),str(W/'pid1.flat')]]
for cmd in commands:subprocess.check_call(cmd,env=env,cwd=str(W),timeout=30)
elf=(W/'pid1.elf').read_bytes();assert elf[:4]==b'\x7fELF';entry=struct.unpack_from('<Q',elf,24)[0]
code=(W/'pid1.flat').read_bytes();assert len(code)<0x4000 and 0x100004000<=entry<0x100004000+len(code)
file_size=(0x4000+len(code)+15)&~15;vm_size=(file_size+0x3fff)&~0x3fff
def segment(name,vm,size,offset,length,prot):return struct.pack('<II16sQQQQiiII',0x19,72,name,vm,size,offset,length,prot,prot,0,0)
state=bytearray(272);struct.pack_into('<Q',state,32*8,entry)
commands=segment(b'__PAGEZERO',0,0x100000000,0,0,0)+segment(b'__TEXT',0x100000000,vm_size,0,file_size,5)+struct.pack('<4I',5,288,6,68)+state
header=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,3,len(commands),1,0)
image=(header+commands).ljust(0x4000,b'\0')+code;image=image.ljust(file_size,b'\0');(W/'pid1.macho').write_bytes(image)
manifest={'source_sha256':hashlib.sha256((W/'pid1.c').read_bytes()).hexdigest(),'linker_script_sha256':hashlib.sha256((W/'pid1.ld').read_bytes()).hexdigest(),'macho_sha256':hashlib.sha256(image).hexdigest(),'elf_sha256':hashlib.sha256(elf).hexdigest(),'bytes':len(image),'crc32':hex(zlib.crc32(image)&0xffffffff),'entry':hex(entry),'architecture':'arm64, 4 KiB-compatible, static, no libc/dyld','syscalls':{'write':4,'open':5,'getpid':20,'getuid':24,'select':93},'syscall_reference':'apple-oss-distributions/xnu xnu-8796.121.2 bsd/kern/syscalls.master','acceptance':['XZPIOS PID1 START pid=1 uid=0','XZPIOS PID1 HEARTBEAT 1','XZPIOS PID1 HEARTBEAT 2','XZPIOS PID1 HEARTBEAT 3'],'executed':False}
(W/'manifest.json').write_text(json.dumps(manifest,indent=2));print(json.dumps(manifest))
