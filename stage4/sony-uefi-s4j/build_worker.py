"""Single-job, private GCC9 build of the Sony headless SEC/DXE probe."""
import hashlib,json,os,struct,subprocess,time
from pathlib import Path
ROOT=Path('/opt/xzpios-lab')
WORK=ROOT/'sony-stage4-20261028-s4j'
BASEWORK=ROOT/'sony-uefi-20261027'
SRC=WORK/'source'
BASESRC=BASEWORK/'source'
EDK=BASESRC/'Common/edk2'
GCC=ROOT/'lineage20-gcc9-build/toolchain'
HOST=ROOT/'lineage20-build/toolchain'
CROSS=WORK/'cross-bin'
CROSS.mkdir(exist_ok=True)
links={p.name:p for p in (GCC/'bin').glob('aarch64-linux-gnu-*')}
for name in ['gcc-ar','gcc-nm','gcc-ranlib']:
 links['aarch64-linux-gnu-'+name]=GCC/('usr/bin/aarch64-linux-gnu-'+name+'-9')
for name,target in links.items():
 link=CROSS/name
 if not link.is_symlink(): link.symlink_to(str(target))
env=os.environ.copy()
env.update(PATH=str(CROSS)+':'+str(GCC/'bin')+':'+str(HOST/'usr/bin')+':/usr/bin:/bin',
 LD_LIBRARY_PATH=':'.join(str(p) for p in [GCC/'usr/lib/x86_64-linux-gnu',HOST/'usr/lib/x86_64-linux-gnu',HOST/'lib/x86_64-linux-gnu']),
 WORKSPACE=str(WORK/'workspace'),PACKAGES_PATH=str(EDK)+':'+str(SRC)+':'+str(BASESRC),
 GCC5_AARCH64_PREFIX=str(CROSS/'aarch64-linux-gnu-'),PYTHON_COMMAND='/usr/bin/python3',
 CPATH=str(BASEWORK/'hostdeps/include'),LIBRARY_PATH=str(BASEWORK/'hostdeps/lib'))
started=time.time()
(WORK/'workspace').mkdir(exist_ok=True)
events=[]
def run(cmd):
 with (WORK/'build.log').open('ab') as out:
  out.write(('\nCOMMAND '+repr(cmd)+'\n').encode()); out.flush()
  p=subprocess.run(cmd,cwd=str(SRC),env=env,stdout=out,stderr=subprocess.STDOUT,timeout=780)
 events.append({'command':cmd,'returncode':p.returncode})
 if p.returncode: raise RuntimeError('Build command failed: '+repr(cmd))
try:
 pins=json.loads((BASEWORK/'fetch-result.json').read_text())
 if pins['status']!='complete' or len(pins['sources'])!=4: raise ValueError('Pinned sources incomplete')
 # DSC lists some optional libraries from external packages; they are not needed by this probe.
 inc=SRC/'SonyProbe/SonyCommon.dsc.inc'
 lines=[]; omitted=[]
 for line in inc.read_text().splitlines():
  if '|' in line and '.inf' in line:
   name=line.split('|',1)[1].strip()
   if not any((p/name).is_file() for p in [SRC,EDK]):
    omitted.append(line);continue
  lines.append(line)
 inc.write_text('\n'.join(lines)+'\n')
 (WORK/'omitted-unused-library-mappings.json').write_text(json.dumps(omitted,indent=2))
 run(['/usr/bin/make','-C',str(EDK/'BaseTools/Source/C'),'-j1'])
 run(['/bin/bash','-c','source "'+str(EDK/'edksetup.sh')+'" BaseTools && build -n 1 -a AARCH64 -t GCC5 -b DEBUG -p SonyProbe/Sony.dsc'])
 fd=WORK/'workspace/Build/SonyResidentProbe/DEBUG_GCC5/FV/MSM8998PKG_UEFI.fd'
 data=fd.read_bytes()
 if len(data)!=0x200000 or data[56:60]!=b'ARMd': raise ValueError('Unexpected FD layout')
 kernel=(ROOT/'stage4d-20261028/kernels/070717724b639c15051d8e02c0dffd1c679c6a4b29e32679fa727e8e89e7d69c.macho').read_bytes()
 if hashlib.sha256(kernel).hexdigest()!='070717724b639c15051d8e02c0dffd1c679c6a4b29e32679fa727e8e89e7d69c': raise ValueError('Kernel hash changed')
 hello=(ROOT/'stage4j-20261028/pid1/pid1.macho').read_bytes()
 if hashlib.sha256(hello).hexdigest()!='b0de20edbb34a3e88ed5392d24e781e3441fb95eb074420176a0dbbc0497df56': raise ValueError('PID1 hash changed')
 data=bytearray(data)
 payload=kernel+hello;payload=payload.ljust((len(payload)+15)&~15,b'\0')
 struct.pack_into('<Q',data,16,len(data)+len(payload))
 fd.write_bytes(data)
 image=WORK/'Image'; image.write_bytes(data+payload)
 result={'image_path':str(image),'image_sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'status':'complete' ,'fd_path':str(fd),'fd_sha256':hashlib.sha256(data).hexdigest(),'fd_bytes':len(data)}
except Exception as e:
 result={'status':'failed','error':repr(e)}
result.update(elapsed_seconds=time.time()-started,events=events)
(WORK/'build-result.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result),flush=True)
if result['status']!='complete': raise SystemExit(1)
