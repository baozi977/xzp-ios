"""Byte-compatible decmpfs forks using one 64 KiB input chunk and a disk spool."""
import os,struct,zlib
def compressed_fork_spooled(path,spool,small,piece,block):
 size=os.path.getsize(path)
 if size<=piece:return small(path)
 spool.seek(0,2);start=spool.tell();count=(size+piece-1)//piece;offset=4+8*count
 spool.seek(start+0x100+4+offset);parts=[];read=0
 with open(path,'rb') as src:
  while True:
   chunk=src.read(piece)
   if not chunk:break
   read+=len(chunk);z=zlib.compress(chunk,9);packed=z if len(z)<len(chunk) else b'\xff'+chunk
   parts.append((offset,len(packed)));spool.write(packed);offset+=len(packed)
 if read!=size or len(parts)!=count:raise RuntimeError('File changed while compressing: '+path)
 data_len=offset+4;map_off=0x100+data_len;fork_len=map_off+50
 if (fork_len+block-1)//block >= (size+block-1)//block:
  spool.truncate(start);spool.seek(start);return None,None
 head=struct.pack('>IIII',0x100,map_off,data_len,50)
 resource_map=bytearray(50);struct.pack_into('>IIII',resource_map,0,0x100,map_off,data_len,50)
 struct.pack_into('>HH',resource_map,24,28,50);struct.pack_into('>H',resource_map,28,0)
 resource_map[30:34]=b'cmpf';struct.pack_into('>HH',resource_map,34,0,10);struct.pack_into('>hHI',resource_map,38,1,0xffff,0)
 spool.seek(start);spool.write(head.ljust(0x100,b'\0'));spool.write(struct.pack('>I',offset));spool.write(struct.pack('<I',count))
 for off,length in parts:spool.write(struct.pack('<II',off,length))
 spool.seek(start+map_off);spool.write(resource_map)
 return 'rsrc-spooled',(start,fork_len)
