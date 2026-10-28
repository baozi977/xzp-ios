"""Generalize the verified stage2 Mach-O layout inspection for rebuilt source kernels."""
import argparse,hashlib,json,struct,zlib
from pathlib import Path
W=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--kernel',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--pid1',type=Path,default=W/'artifacts/pid1-heartbeat-20261027/pid1.macho');a=ap.parse_args()
raw=a.kernel.read_bytes();pid=a.pid1.read_bytes();magic,cpu,subtype,typ,count,cmdbytes,flags,res=struct.unpack_from('<8I',raw)
assert magic==0xfeedfacf and cpu==0x100000c and typ==2 and count<4096 and 32+cmdbytes<=len(raw)
off=32;segments=[];entry=None
for _ in range(count):
 cmd,size=struct.unpack_from('<II',raw,off);assert size>=8 and off+size<=32+cmdbytes
 if cmd==0x19:
  _,_,name,vm,vs,fo,fs,maxp,prot,nsect,sflags=struct.unpack_from('<II16s4QiiII',raw,off);assert fo+fs<=len(raw) and fs<=vs
  segments.append({'name':name.rstrip(b'\0').decode(),'vmaddr':vm,'vmsize':vs,'fileoff':fo,'filesize':fs,'protection':prot})
 if cmd==5:
  flavor,n=struct.unpack_from('<II',raw,off+8);assert flavor==6 and n>=66;entry=struct.unpack_from('<Q',raw,off+16+32*8)[0]
 off+=size
assert off==32+cmdbytes and entry and len(segments)<=32
base=0xa4000000;virt=0xfffffff004000000;window=0x10000000
for s in segments:
 assert virt<=s['vmaddr']<=virt+window and s['vmsize']<=virt+window-s['vmaddr']
for i,s in enumerate(segments):
 for t in segments[:i]:assert not s['vmsize'] or not t['vmsize'] or s['vmaddr']+s['vmsize']<=t['vmaddr'] or t['vmaddr']+t['vmsize']<=s['vmaddr']
assert any(s['vmaddr']<=entry<s['vmaddr']+s['vmsize'] and s['protection']&4 for s in segments)
align=lambda v,n:(v+n-1)&~(n-1)
rd=align(base+max(s['vmaddr']+s['vmsize'] for s in segments)-virt,0x4000);rdsize=align(len(pid),0x4000);dt=rd+rdsize;args=dt+0x10000;top=align(args+736,0x4000);sourcecopy=align(len(raw)+len(pid),16)
assert top<=base+window and sourcecopy<=0x1000000
assert 0x80080000+0x200000+sourcecopy<=0x83200000
contract=json.loads((W/'captures/sony-boot-contract-20261026-01/sony-boot-contract.json').read_text())
for start,end in [(base,base+window),(0xf0000000,0xf1000000),(0xf1000000,0xf1080000)]:
 for reserved in contract['fixed_reserved_memory']:assert end<=int(reserved['start'],16) or start>=int(reserved['end_exclusive'],16)
values={'SOURCE_SIZE':len(raw),'SOURCE_CRC32':zlib.crc32(raw)&0xffffffff,'XNU_SEGMENT_COUNT':len(segments),'XNU_RAMDISK':rd,'XNU_RAMDISK_SIZE':rdsize,'XNU_DT':dt,'XNU_ARGS':args,'XNU_TOP':top,'XNU_ENTRY':base+entry-virt,'SOURCE_COPY_SIZE':sourcecopy}
sha=hashlib.sha256(raw).hexdigest()
if sha=='9239d4adb7a2c433d7ed5729b47b63acc5281af73d8a81640ef3c678c8afd26c':
 for key,expected in {'SOURCE_SIZE':15623056,'SOURCE_CRC32':0x3f699480,'XNU_RAMDISK':0xa82e4000,'XNU_RAMDISK_SIZE':0x8000,'XNU_DT':0xa82ec000,'XNU_ARGS':0xa82fc000,'XNU_TOP':0xa8300000,'XNU_ENTRY':0xa745e4a8,'SOURCE_COPY_SIZE':0xeea610}.items():assert values[key]==expected,key
a.out.mkdir(parents=True,exist_ok=True)
header='// Generated from the actual Mach-O and measured Sony memory contract.\n#pragma once\n'+''.join('#define %s 0x%xULL\n'%(k,v) for k,v in values.items())
(a.out/'KernelContract.h').write_bytes(header.encode());r={'kernel_sha256':sha,'kernel_bytes':len(raw),'kernel_path':str(a.kernel.resolve()),'pid1_sha256':hashlib.sha256(pid).hexdigest(),'entry_va':hex(entry),'values':{k:hex(v) for k,v in values.items()},'segments':[{**s,'vmaddr':hex(s['vmaddr'])} for s in segments],'matches_native_r21_contract':sha=='9239d4adb7a2c433d7ed5729b47b63acc5281af73d8a81640ef3c678c8afd26c'};(a.out/'kernel-contract.json').write_text(json.dumps(r,indent=2));print(json.dumps({k:v for k,v in r.items() if k!='segments'},indent=2))
