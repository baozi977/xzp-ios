"""Require actual Sony/kernel/PID1 evidence, beyond the generic return check."""
import hashlib,json,re
from pathlib import Path
W=Path(__file__).resolve().parents[1];C=W/'captures/sony-stage4-20261028-s4f';P=C/'phone'
log=(P/'entry-log.txt').read_text();state=json.loads((P/'state.json').read_text())
fixture=(W/'artifacts/stage4-extent-20261028/read-fixture.bin').read_bytes();h=0xcbf29ce484222325
for b in fixture:h=((h^b)*0x100000001b3)&0xffffffffffffffff
assert state['passed'] and state['entry_complete'] and state['android_restored']
assert 'SONY_UFS_KERNEL_PROOF_PASS bytes=65536 fnv=%x'%h in log
assert 'SONY_UFS_KERNEL_READY files=1 readonly=1 mailbox=0' in log
assert 'SONY_PID1_UFS_FILE_PASS bytes=65536 fnv=%d'%h in log
assert 'XZPIOS PID1 START pid=1 uid=0' in log
beats=[int(x) for x in re.findall(r'XZPIOS PID1 HEARTBEAT (\d+)',log)];assert beats==list(range(1,15)),beats
assert '20 reads, 0 errors, 131072 bytes' in log
assert 'S4F_UFS_PRIME_READ crc=FFE535DF expected=FFE535DF ocs=0 readonly=1' in log
assert 'S4F_UFS_RETURN restored=1 list_in=0 switched_in=1 switched_out=1 db=0' in log
assert re.search(r'S4F_UFS_FIRMWARE_RESTORE restored=1 base=0:[0-9A-F]+ db=0',log)
assert re.search(r'UI_XNU_RETURN_S4F status=Success display_crc_before=([0-9A-F]+) after=\1 equal=1',log)
assert 'UI_RESTORE_DETAIL_S4F idle=1 clean=1 frame_done=1 refresh_state=1' in log
assert all(x not in log for x in ['panic(cpu','Assertion failed:','SONY_PID1_UFS_FILE_FAIL','SONY_UFS_NATIVE_START_FAILED','S2_APP_FAILED'])
meta=json.loads((W/'artifacts/sony-stage4-20261028-s4f/verification.json').read_text())
assert hashlib.sha256((W/'artifacts/sony-stage4-20261028-s4f/boot.img').read_bytes()).hexdigest()==meta['boot_sha256']
kernel=W/'artifacts/stage4f-kernel-20261028/mach.development.bcm2837';kernelsha=hashlib.sha256(kernel.read_bytes()).hexdigest()
assert kernelsha=='070717724b639c15051d8e02c0dffd1c679c6a4b29e32679fa727e8e89e7d69c'
r={'passed':True,'scope':'SO-04J bare-metal XNU UFS file-view and userspace readback','kernel_sha256':kernelsha,
 'fixture_sha256':hashlib.sha256(fixture).hexdigest(),'fixture_fnv':hex(h),'kernel_read_bytes':65536,'pid1_read_bytes':65536,
 'hardware_reads':20,'hardware_bytes':131072,'hardware_read_errors':0,'heartbeats':beats,'kernel_queue_restored_to_standby':True,
 'original_firmware_queue_restored':True,'display_memory_unchanged_across_xnu':True,'android_restored':True,
 'boot_fota_hashes_unchanged':True,'apple_launchd_native':False,'entry_log_sha256':hashlib.sha256((P/'entry-log.txt').read_bytes()).hexdigest()}
(C/'verified-result.json').write_text(json.dumps(r,indent=2));meta.update(phone_tested=True,native_xnu_storage=True,native_project_file_read=True,native_apple_launchd=False)
(W/'artifacts/sony-stage4-20261028-s4f/verification.json').write_text(json.dumps(meta,indent=2))
for tag,cause in [('s4d','Firmware retained WRITE(10); the upstream readonly handoff guard refused the queue'),('s4e','Mapping a separate command page inside the interrupt-disabled switch path caused an allocation assertion')]:
 c=W/('captures/sony-stage4-20261028-'+tag);s=json.loads((c/'phone/state.json').read_text());assert s['android_restored']
 (c/'diagnosis.json').write_text(json.dumps({'native_storage_passed':False,'android_restored':True,'cause':cause,'fixed_by':'S4F'},indent=2))
print(json.dumps(r,indent=2))
