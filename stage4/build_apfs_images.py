"""Reuse the pinned userland recipe with bounded streaming compression."""
import hashlib,importlib.util,json,os,shutil,subprocess,time,traceback
from pathlib import Path
R=Path('/opt/xzpios-lab');T=R/'stage4-20261028';W=T/'userland-workspace';start=time.time();events=[]
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while True:
   b=f.read(1<<20)
   if not b:break
   h.update(b)
 return h.hexdigest()
def run(cmd,cwd,timeout=800):
 with (T/'apfs-build.log').open('ab') as f:
  f.write(('\nCOMMAND '+repr(cmd)+'\n').encode());f.flush()
  p=subprocess.run(cmd,cwd=str(cwd),env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 events.append({'command':cmd,'returncode':p.returncode})
 if p.returncode:raise RuntimeError('Image recipe failed')
try:
 assert json.loads((T/'extract-result.json').read_text())['status']=='complete'
 assert shutil.disk_usage(T).free>8*1024**3,'Need space for compressed spool and final image'
 source=W/'tools/apfs/mkapfs.py';ref=R/'stage3-20261027/sources/hackios-userland/tools/apfs/mkapfs.py'
 original=ref.read_text();current=source.read_text();assert current==original or 'rsrc-spooled' in current
 # Import the original encoder for byte-for-byte comparison on small fixtures.
 spec=importlib.util.spec_from_file_location('original_apfs',str(ref));old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
 spec=importlib.util.spec_from_file_location('stream_apfs',str(T/'apfs_stream.py'));stream=importlib.util.module_from_spec(spec);spec.loader.exec_module(stream)
 tests=T/'apfs-tests';tests.mkdir(exist_ok=True)
 for size,random in [(1,False),(65536,False),(65537,False),(300000,False),(100000,True),(400000,True)]:
  path=tests/('input-%d-%d'%(size,random));data=os.urandom(size) if random else bytes((i*31+(i>>8))&255 for i in range(size));path.write_bytes(data)
  kind,want=old.compressed_fork(str(path))
  with (tests/'spool').open('w+b') as spool:
   gotkind,got=stream.compressed_fork_spooled(str(path),spool,old.compressed_fork,old.PIECE,old.BS)
   if gotkind=='rsrc-spooled':spool.seek(got[0]);got=spool.read(got[1]);gotkind='rsrc'
   assert (gotkind,got)==(kind,want),(size,random)
 if current==original:
  text=original.replace('import argparse','import argparse\nfrom apfs_stream import compressed_fork_spooled',1)
  before='''            pool = concurrent.futures.ProcessPoolExecutor()
            forks = pool.map(compressed_fork, [n.source for n in files if n.size > 0 and not self.is_plain(n)],
                             chunksize=16)'''
  assert before in text;text=text.replace(before,'''            forks = (compressed_fork_spooled(n.source, self.spool, compressed_fork, PIECE, BS)
                     for n in files if n.size > 0 and not self.is_plain(n))''')
  before="                if kind == 'rsrc':";assert before in text
  text=text.replace(before,"""                if kind == 'rsrc-spooled':
                    sid = self.new_oid()
                    self.data[sid] = ('spool', rsrc[0], rsrc[1])
                    self.decmpfs[node.oid] = (struct.pack('<IIQ', DECMPFS_MAGIC, DECMPFS_ZLIB_RSRC, node.size), sid)
                    continue
"""+before)
  text=text.replace('        if self.compress:\n            pool.shutdown()\n','')
  before='                        f.write(v.spool.read(size))';assert before in text
  text=text.replace(before,'''                        left = size
                        while left:
                            chunk = v.spool.read(min(left, 1 << 22))
                            if not chunk: raise RuntimeError('Truncated compression spool')
                            f.write(chunk)
                            left -= len(chunk)''')
  source.write_text(text)
 shutil.copy2(str(T/'apfs_stream.py'),str(W/'tools/apfs/apfs_stream.py'))
 # Same two prebuilt-host compatibility changes used in stage three.
 p=W/'userland/build.sh';text=p.read_text()
 if 'cp obj/hello_dyn obj/svc_test obj/surf_test obj/drawable_test obj/rwtouch obj/mg_test root/bin/' in text:
  text=text.replace('cp obj/hello_dyn obj/svc_test obj/surf_test obj/drawable_test obj/rwtouch obj/mg_test root/bin/',
   'cp obj/hello_dyn obj/svc_test obj/surf_test obj/rwtouch obj/mg_test root/bin/\n\t[ -f obj/drawable_test ] && cp obj/drawable_test root/bin/')
 text=text.replace('zone=$(timedatectl show -p Timezone --value 2>/dev/null);','zone=$(timedatectl show -p Timezone --value 2>/dev/null || true);');p.write_text(text)
 env=os.environ.copy();env.update(PREBUILT='1',WIFID='0',SYSTEM='1',JOBS='com.apple.notifyd com.apple.cfprefsd.xpc.daemon com.apple.cfprefsd.xpc.daemon.system',
  THEOS=str(R/'stage3-20261027/theos'),PATH='/usr/bin:/bin',PYTHONDONTWRITEBYTECODE='1')
 for key in ['GL','ADSP','FLASHBOOT','CRYPTEXES','JB','EXTRA','EXTRA_SHARE']:env.pop(key,None)
 # pipefail makes compression failures visible instead of accepting tail's status.
 run(['/bin/bash','-o','pipefail','build.sh'],W/'userland')
 images={}
 for name in ['root.apfs','system.apfs']:
  path=W/'userland'/name
  with path.open('rb') as f:f.seek(32);assert f.read(4)==b'NXSB'
  images[name]={'bytes':path.stat().st_size,'sha256':sha(path),'path':str(path)}
 r={'status':'complete','images':images,'stream_encoder_tests':6,'mkapfs_reference_sha256':sha(ref),'mkapfs_private_sha256':sha(source),
  'compression':'Serial 64 KiB blocks, file-backed resource forks; byte-compatible with upstream encoder',
  'free_bytes':shutil.disk_usage(T).free,'jobs':env['JOBS']}
except Exception as e:r={'status':'failed','error':str(e),'traceback':traceback.format_exc()}
r.update(events=events,elapsed_seconds=time.time()-start);(T/'apfs-build-result.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
if r['status']!='complete':raise SystemExit(1)
