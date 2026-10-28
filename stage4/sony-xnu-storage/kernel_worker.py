"""Incremental private XNU build; preserve R22 source/binary after collecting S4D."""
import hashlib,json,os,shutil,subprocess,time
from pathlib import Path
R=Path('/opt/xzpios-lab');OUT=R/'stage4d-20261028';W=R/'stage3-20261027'
S=W/'build/hackios/xnu-src/xnu-8796.121.2';overlay=OUT/'overlay';backup=OUT/'reference';backup.mkdir(exist_ok=True)
kernel=S/'BUILD/obj/DEVELOPMENT_ARM64_BCM2837/mach.development.bcm2837'
originals={};added=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def atomic(p,raw):
 tmp=p.with_name(p.name+'.stage4tmp');tmp.write_bytes(raw);os.replace(str(tmp),str(p))
try:
 assert sha(kernel)=='93919f7e75100f0d39db35c233ca908e918a7ae74f42f6bd253b641908854c53'
 if not (backup/'r22.macho').exists():shutil.copy2(str(kernel),str(backup/'r22.macho'))
 # Exercise the actual extent mapper with non-adjacent physical regions.
 subprocess.check_call(['/usr/bin/cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(overlay/'test_extent.c'),'-o',str(OUT/'test_extent')],timeout=20)
 subprocess.check_call([str(OUT/'test_extent')],timeout=5)
 for name in ['redwood_ufs.c','redwood_ufshc.c']:
  p=S/'bsd/dev'/name;originals[name]=p.read_bytes()
  pin=R/'stage3-20261027/sources/hackios-kernel/xnu/files/bsd/dev'/name
  assert originals[name]==pin.read_bytes(),'Expected pristine pinned UFS source'
  (backup/name).write_bytes(originals[name])
 for name in ['SonyUfsContract.h','SonyExtentRead.h','SonyUfsFile.h']:
  p=S/'bsd/dev'/name;assert not p.exists(),'Unexpected overlay file in reference tree'
  shutil.copy2(str(overlay/name),str(p));added.append(p)
 t=originals['redwood_ufs.c'].decode()
 needle='static volatile bool ufs_native_on;';assert t.count(needle)==1
 t=t.replace(needle,needle+'\n#include "SonyUfsFile.h"')
 needle='ufs_read(uint32_t part, uint64_t offset, uint8_t *dst, uint64_t len)\n{';assert needle in t
 t=t.replace(needle,needle+'\n\tif (sony_offered) return sony_file_read(part, offset, dst, len);')
 needle='\tif (!PE_parse_boot_argn("ufs_shared_pa", &pa, sizeof(pa)) || pa == 0) {';assert needle in t
 t=t.replace(needle,'\tif (PE_parse_boot_argn("sony_ufs_context", &pa, sizeof(pa)) && pa != 0) {\n\t\tif (!sony_prepare(pa)) { printf("SONY_UFS_CONTEXT rejected\\n"); return; }\n\t\tgoto sony_devices;\n\t}\n'+needle)
 needle='\tufs_bmajor = bdevsw_add(-1, &ufs_bdevsw);';assert needle in t
 t=t.replace(needle,'sony_devices:\n'+needle)
 needle='\t/*\n\t * ufs_write=1: /dev/rufs_darwin takes writes';assert needle in t
 t=t.replace(needle,'\tif (sony_offered) {\n\t\tif (!sony_start_native()) { ufs_ready=false; printf("SONY_UFS_NATIVE_START_FAILED\\n"); }\n\t\treturn;\n\t}\n\n'+needle)
 atomic(S/'bsd/dev/redwood_ufs.c',t.encode())
 t=originals['redwood_ufshc.c'].decode()
 needle='#define UFSHC_PHYS          0x1d84000ULL';assert needle in t
 t=t.replace(needle,'static uint64_t hc_phys = 0x1d84000ULL;\n#define UFSHC_PHYS          hc_phys')
 needle='static vm_offset_t hc_uefi_map_va;';assert needle in t
 t=t.replace(needle,needle+'\nstatic uint64_t sony_cd_page;\nstatic vm_offset_t sony_cd_va;')
 needle='\tif (!hc_whitelist_selftest()) {';assert needle in t
 t=t.replace(needle,'\tuint64_t sony_context_pa=0;\n\tif (PE_parse_boot_argn("sony_ufs_context", &sony_context_pa, sizeof(sony_context_pa)) && sony_context_pa) hc_phys=0x01da4000ULL;\n\n'+needle)
 old='''\tif (ucd < hc_uefi_map_pa || ucd + 32 > hc_uefi_map_pa + hc_uefi_map_size) {
\t\treturn "UEFI's command descriptor is not where it was";
\t}
\tu = (volatile const uint8_t *)(hc_uefi_map_va + (ucd - hc_uefi_map_pa));'''
 new='''\tif (ucd < hc_uefi_map_pa || ucd + 32 > hc_uefi_map_pa + hc_uefi_map_size) {
\t\tif (hc_phys!=0x01da4000 || (ucd&127) || !sony_cd_va || (ucd&~(uint64_t)PAGE_MASK)!=sony_cd_page)
\t\t\treturn "Sony firmware command descriptor moved from its prevalidated page";
\t\tu=(volatile const uint8_t *)(sony_cd_va+(ucd&PAGE_MASK));
\t} else u = (volatile const uint8_t *)(hc_uefi_map_va + (ucd - hc_uefi_map_pa));'''
 assert old in t;t=t.replace(old,new)
 needle='\t\tno = hc_uefi_slot0(uefi_upiu, &uefi_dw0);\n\t\thc_say("ufs: native: UEFI\'s last command:'
 assert needle in t
 premap='''\t\t/* Map the Sony command page here, while allocation is allowed. switch_in
\t\t * later validates the same page with interrupts disabled, without allocating. */
\t\tif (hc_phys==0x01da4000) {
\t\t\tuint64_t ucd=(uint64_t)hc_uefi_list[5]<<32|hc_uefi_list[4];
\t\t\tif (!(ucd&127) && ucd>=0x80000000ULL && ucd<=0x17e4c0000ULL-32 &&
\t\t\t    !pmap_valid_address(ucd) && !pmap_valid_address(ucd+31)) {
\t\t\t\tsony_cd_page=ucd&~(uint64_t)PAGE_MASK;
\t\t\t\tsony_cd_va=ml_io_map_unmappable(sony_cd_page,PAGE_SIZE,VM_WIMG_WCOMB);
\t\t\t}
\t\t}
'''
 t=t.replace(needle,premap+needle);atomic(S/'bsd/dev/redwood_ufshc.c',t.encode())
 # Keep the exact final patched sources beside the build manifest.
 patched=OUT/'patched';patched.mkdir(exist_ok=True)
 for name in originals:shutil.copy2(str(S/'bsd/dev'/name),str(patched/name))
 code=(W/'kernel-build_worker.py').read_text();assert "phase='kernel'" in code
 code=code.replace("started=time.time();r=", "OUT=R/'stage4d-20261028'\nstarted=time.time();r=")
 code=code.replace('(W/(key','(OUT/(key')
 needle="  r['status']='complete'\nexcept"
 assert needle in code
 code=code.replace(needle,"  import shutil\n  saved=OUT/'mach.development.bcm2837'\n  shutil.copy2(str(kernel),str(saved));r['kernel_path']=str(saved)\n  r['sony_storage_overlay']=True\n  r['status']='complete'\nexcept")
 exec(compile(code,str(OUT/'bounded-kernel-build.py'),'exec'),{'__name__':'__stage4_build__'})
finally:
 for name,raw in originals.items():atomic(S/'bsd/dev'/name,raw)
 for p in added:
  if p.exists():p.unlink()
 if (backup/'r22.macho').exists():shutil.copy2(str(backup/'r22.macho'),str(kernel))
 (OUT/'reference-restored.json').write_text(json.dumps({'source_sha256':{n:sha(S/'bsd/dev'/n) for n in originals},'reference_kernel_sha256':sha(kernel)},indent=2))
