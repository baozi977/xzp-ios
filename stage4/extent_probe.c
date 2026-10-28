// SPDX-License-Identifier: MIT
// Freestanding AArch64 Linux/Android helper: readonly FIEMAP, no libc or keys.
typedef unsigned long U64;
typedef unsigned int U32;
struct Extent {U64 logical,physical,length,reserved64[2];U32 flags,reserved[3];};
struct Map {U64 start,length;U32 flags,mapped,count,reserved;struct Extent e[256];};
_Static_assert(sizeof(struct Extent)==56,"Linux fiemap extent ABI");
_Static_assert(__builtin_offsetof(struct Map,e)==32,"Linux fiemap header ABI");
static struct Map map;
static long call(long n,long a,long b,long c){
 register long x0 __asm__("x0")=a;register long x1 __asm__("x1")=b;
 register long x2 __asm__("x2")=c;register long x8 __asm__("x8")=n;
 __asm__ volatile("svc #0":"+r"(x0):"r"(x1),"r"(x2),"r"(x8):"memory","cc");return x0;
}
static void say(const char *s){U64 n=0;while(s[n])n++;call(64,1,(long)s,n);}
static void num(U64 v){char b[32];int p=32;do{b[--p]='0'+v%10;v/=10;}while(v);call(64,1,(long)(b+p),32-p);}
static long fail(const char *why,long err){say("{\"error\":\"");say(why);say("\",\"errno\":");num(err<0?-err:err);say("}\n");return 1;}
long entry(U64 *stack){
 long fd,r,size;U64 flags=0;U32 i;char **argv=(char **)(stack+1);
 if(stack[0]!=2)return fail("need one file path",0);
 fd=call(56,-100,(long)argv[1],0);if(fd<0)return fail("open",fd);
 size=call(62,fd,0,2);if(size<0)return fail("size",size);
 r=call(29,fd,0x80086601,(long)&flags);if(r<0)return fail("getflags",r);
 if(flags&0x800)return fail("file is encrypted",0);
 map.length=~0UL;map.flags=1;map.count=256;
 r=call(29,fd,0xc020660b,(long)&map);if(r<0)return fail("fiemap",r);
 call(57,fd,0,0);
 if(!map.mapped || map.mapped>256 || !(map.e[map.mapped-1].flags&1))return fail("incomplete map",0);
 say("{\"bytes\":");num(size);say(",\"file_flags\":");num(flags);say(",\"extents\":[");
 for(i=0;i<map.mapped;i++){
  if(i)say(",");say("{\"logical\":");num(map.e[i].logical);say(",\"physical\":");num(map.e[i].physical);
  say(",\"length\":");num(map.e[i].length);say(",\"flags\":");num(map.e[i].flags);say("}");
 }
 say("]}\n");return 0;
}
__asm__(".text\n.global _start\n.type _start,%function\n_start:\nmov x0,sp\nbl entry\nmov x8,#93\nsvc #0\n");
