import json
from pathlib import Path
W=Path(__file__).resolve().parents[1];O=W/'stage4/sony-uefi-s4j'
board=json.loads((W/'captures/sony-boot-contract-20261026-01/sony-boot-contract.json').read_text())
for r in board['fixed_reserved_memory']:assert 0xe8000000<=int(r['start'],16) or 0xa4000000>=int(r['end_exclusive'],16),r
assert 0xa4000000+0x44000000==0xe8000000<0xf0000000
for n in ['KernelStage.h','SonyMemoryInit.c']:
 p=O/n;t=p.read_text();assert '0x40000000ULL' in t;t=t.replace('0x40000000ULL','0x44000000ULL');p.write_bytes(t.encode())
p=O/'SonyUfsPrepare.h';t=p.read_text();t=t.replace('STATIC EFI_PHYSICAL_ADDRESS SonyPrime;','STATIC EFI_PHYSICAL_ADDRESS SonyPrime,SonyContextAllocation;')
needle='Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,1,&Pa);if(EFI_ERROR(Status))return Status;';assert needle in t
t=t.replace(needle,'Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,7,&SonyContextAllocation);if(EFI_ERROR(Status))return Status;\n Pa=(SonyContextAllocation+16383)&~16383ULL;')
t=t.replace('gBS->FreePages(Pa,1)','gBS->FreePages(SonyContextAllocation,7)').replace('gBS->FreePages(SonyUfsContextPa,1)','gBS->FreePages(SonyContextAllocation,7)')
# A 16 KiB software-page mapping must be fully backed by this allocation.
t=t.replace('if(Pa<0xa0200000 || Pa+4096>0xa3fc0000','if(SonyContextAllocation<0xa0200000 || SonyContextAllocation+7*4096>0xa3fc0000 || Pa+16384>SonyContextAllocation+7*4096 || Pa<0xa0200000 || Pa+16384>0xa3fc0000')
p.write_bytes(t.encode())
(O/'page-policy.json').write_text(json.dumps({'native_memory_bytes':0x44000000,'native_end':'0xe8000000','hardware_page_bytes':4096,'expected_vm_page_bytes':16384,'policy':'Unmodified arm_init chooses PAGE_MAX_SHIFT when memSize > 1 GiB','context_allocation_pages_4k':7,'context_alignment_bytes':16384,'kernel_and_apple_images_unchanged':True},indent=2))
print('S4J: 1088 MiB, 4 KiB hardware / 16 KiB VM; aligned context with complete owned 16 KiB backing.')
