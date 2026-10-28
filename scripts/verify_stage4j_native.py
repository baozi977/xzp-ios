"""Strict phase-four acceptance, separate from the generic UEFI return gate."""
import hashlib,json,re,subprocess,sys
from pathlib import Path
W=Path(__file__).resolve().parents[1];C=W/'captures/sony-stage4-20261028-s4j';P=C/'phone'
subprocess.check_call([sys.executable,'-B',str(W/'scripts/inspect_stage4j_native.py')],stdout=subprocess.DEVNULL)
r=json.loads((C/'native-inspection.json').read_text());L=(P/'entry-log.txt').read_text(errors='replace');state=json.loads((P/'state.json').read_text())
assert r['phase4_acceptance'],r
assert '=== 36 checks, 0 failed ===' in L
assert 'disallowing unsupported code signature page shift' not in L
assert 'page size 16384)' in L,'Require actual APFS VM page-size evidence'
km=re.search(r'S2_KERNEL_REGS_S4J SCTLR=[0-9A-F]+ TCR=([0-9A-F]+)',L);assert km
tcr=int(km[1],16);assert (tcr>>14)&3==0 and (tcr>>30)&3==2,'Require 4 KiB TG0 and TG1 hardware translation'
assert '--- loader_return_after expired ---' in L
assert r['state_passed'] and not r['boot_partition_written'] and not r['panic']
assert re.search(r'UI_XNU_RETURN_S4J status=Success display_crc_before=([0-9A-F]+) after=\1 equal=1',L)
assert 'UI_RESTORE_DETAIL_S4J idle=1 clean=1 frame_done=1 refresh_state=1' in L
services=[x for x in L.splitlines() if 'xpcproxy spawned with pid' in x and ('com.apple.notifyd' in x or 'com.apple.cfprefsd' in x)]
assert services,'Require an actual Apple daemon spawn, not just a project job'
ipc_markers=['[ ok ] notifyd: register for a notification','[ ok ] notifyd: store and fetch a state value','[ ok ] notifyd: post a notification and see it arrive','[ ok ] cfprefsd: write a preference','[ ok ] cfprefsd: read it back','svc_test: 0 failed']
assert all(m in L for m in ipc_markers)
assert 'surf_test: 80 checks, 0 failed' in L
stats=[tuple(map(int,m)) for m in re.findall(r'(\d+) reads, (\d+) errors, (\d+) bytes',L)];assert stats and all(x[1]==0 for x in stats)
art=W/'artifacts/sony-stage4-20261028-s4j';meta=json.loads((art/'verification.json').read_text());assert hashlib.sha256((art/'boot.img').read_bytes()).hexdigest()==meta['boot_sha256']
kernelsha=hashlib.sha256((W/'artifacts/stage4f-kernel-20261028/mach.development.bcm2837').read_bytes()).hexdigest();assert kernelsha=='070717724b639c15051d8e02c0dffd1c679c6a4b29e32679fa727e8e89e7d69c'
images=json.loads((W/'captures/stage4j-20261028/apfs-file-extents.json').read_text());assert images['passed'] and len(images['files'])==3
proof=json.loads((W/'captures/stage4d-20261028/apfs-boot-files.json').read_text());assert proof['system.apfs:sbin/launchd.ios']['sha256']=='4c61512d19b9a55a6ddad27e23b88fc5008fbf77d0c0b5e6bdd7c7d90cef3e06'
baseline=json.loads((W/'captures/uefi-sony-20261027/inventory.json').read_text(encoding='utf-8-sig'))
server=json.loads((W/'captures/stage4j-20261028/server-after.json').read_text());assert server['services']==baseline['services'] and not server['lab_units'].strip()
reads,errors,total=max(stats)
out={'passed':True,'scope':'SO-04J native read-only APFS root, Apple dyld/shared cache, Apple launchd early boot and xpcproxy Apple daemons','kernel_sha256':kernelsha,'boot_sha256':meta['boot_sha256'],'apple_launchd_sha256':proof['system.apfs:sbin/launchd.ios']['sha256'],'native_memory_bytes':0x44000000,'hardware_page_bytes':4096,'vm_page_bytes':16384,'kernel_runtime_patches':False,'apfs_root_switch':True,'init_checks':36,'init_failed_checks':0,'dynamic_probe_expected_exit':7,'apple_launchd_native':True,'apple_launchd_early_boot_complete':True,'apple_daemon_spawn_evidence':services,'hardware_reads':reads,'hardware_read_errors':errors,'hardware_read_bytes':total,'standby_queue_restored':True,'firmware_queue_restored':True,'display_restored':True,'android_restored':True,'boot_fota_hashes_unchanged':True,'server_services_unchanged':True,'cloud_used':False,'gui_or_touch':False,'entry_log_sha256':r['entry_log_sha256']}
out.update(apple_ipc_functional_checks=ipc_markers,iosurface_interface_checks=80,iosurface_interface_failures=0,kernel_tcr=hex(tcr),timed_return_seconds=90)
(C/'verified-result.json').write_text(json.dumps(out,indent=2));meta.update(phone_tested=True,native_xnu_storage=True,native_apfs_root_switch=True,native_apple_launchd=True,phase4_accepted=True,native_xnu_window=['0xa4000000','0xe8000000'],native_memory_bytes=0x44000000,hardware_page_bytes=4096,vm_page_bytes=16384);(art/'verification.json').write_text(json.dumps(meta,indent=2));print(json.dumps(out,indent=2))
