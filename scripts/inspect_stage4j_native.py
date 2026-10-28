import hashlib,json,re
from pathlib import Path
W=Path(__file__).resolve().parents[1];C=W/'captures/sony-stage4-20261028-s4j';P=C/'phone';L=(P/'entry-log.txt').read_text(errors='replace');s=json.loads((P/'state.json').read_text())
markers=['SONY_APFS_BOOTSTRAP_S4J pid=1 uid=0','SONY_UFS_KERNEL_READY files=3 readonly=1 mailbox=0','S2_XNU_PAGE_SWEEP_PASS_S4J pages=278528','SONY_APFS_MOUNT_SYSTEM result=0','SONY_APFS_APPLE_LAUNCHD_PROOF result=0','SONY_APFS_SWITCH_ROOT result=0','SONY_APFS_MOUNT_TOOLS result=0','SONY_APFS_INIT_PROOF result=0','SONY_APFS_EXEC_INIT']
r={'state_passed':s.get('passed'), 'android_restored':s.get('android_restored'),'boot_partition_written':s.get('boot_partition_written'),'markers':{m:m in L for m in markers},'panic':'panic(cpu' in L or 'Assertion failed:' in L,'storage_reports':[x for x in L.splitlines() if re.search(r'\d+ reads, \d+ errors, \d+ bytes',x)],'launchd_lines':[x[:1200] for x in L.splitlines() if any(t in x for t in ['SONY_APFS_','dyld','launchd','xpcproxy','killed by signal','checks,','error','panic(cpu'])][-120:],'standby_queue_restored':'S4J_UFS_RETURN restored=1 list_in=0 switched_in=1 switched_out=1 db=0' in L,'firmware_queue_restored':'S4J_UFS_FIRMWARE_RESTORE restored=1' in L,'entry_log_sha256':hashlib.sha256((P/'entry-log.txt').read_bytes()).hexdigest()}
r['apfs_root_switch_passed']=all(r['markers'].values()) and not r['panic'] and r['standby_queue_restored'] and r['firmware_queue_restored'] and r['android_restored']
r['apple_dynamic_probe_passed']='[ ok ] run a program through dyld and the shared cache' in L and 'exited with status 7' in L
r['apple_launchd_executed']='Darwin Bootstrapper Version' in L and 'launchd.ios' in L
r['apple_launchd_early_boot_complete']='Early boot complete. Continuing system boot.' in L
r['apple_services_spawned_via_xpcproxy']='xpcproxy spawned with pid' in L
r['phase4_acceptance']=r['apfs_root_switch_passed'] and all(r[k] for k in ['apple_dynamic_probe_passed','apple_launchd_executed','apple_launchd_early_boot_complete','apple_services_spawned_via_xpcproxy'])
(C/'native-inspection.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))
