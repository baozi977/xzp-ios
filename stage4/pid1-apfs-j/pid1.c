// SPDX-License-Identifier: MIT
// Static PID1: mount project APFS files read-only, switch root, reuse pinned init.
typedef unsigned long U64;typedef long S64;
static S64 Call(S64 N,S64 A,S64 B,S64 C,S64 D,S64 E,S64 F){
 register S64 x16 __asm__("x16")=N,x0 __asm__("x0")=A,x1 __asm__("x1")=B,x2 __asm__("x2")=C;
 register S64 x3 __asm__("x3")=D,x4 __asm__("x4")=E,x5 __asm__("x5")=F;
 __asm__ volatile("svc #0x80\n cneg %0,%0,cs":"+r"(x0),"+r"(x1),"+r"(x2),"+r"(x3),"+r"(x4),"+r"(x5),"+r"(x16)::"memory","cc");return x0;
}
static U64 Len(const char *S){U64 N=0;while(S[N])N++;return N;}
static void Put(S64 F,const char *S){Call(4,F,(S64)S,Len(S),0,0,0);}
static void Num(S64 F,U64 V){char B[24];U64 I=23;B[I]=0;do{B[--I]='0'+V%10;V/=10;}while(V);Put(F,B+I);}
static void Log(S64 F,const char *S,S64 V){Put(F,S);if(V<0)Put(F,"-");Num(F,V<0?-V:V);Put(F,"\n");}
static void Sleep(void){struct{S64 S;int Us,Pad;}T={1,0,0};Call(93,0,0,0,0,(S64)&T,0);}
static __attribute__((noreturn)) void Stop(S64 F){for(U64 I=1;;I++){Sleep();if(I<=16)Log(F,"XZPIOS PID1 HEARTBEAT ",I);}}
static S64 Proof(S64 F,const char *Path,U64 Expected,U64 Bytes){
 char B[4096];S64 D=Call(5,(S64)Path,0,0,0,0,0);U64 Total=0,H=0xcbf29ce484222325ULL;
 if(D<0)return D;
 while(Total<Bytes){U64 Want=Bytes-Total;if(Want>sizeof(B))Want=sizeof(B);S64 N=Call(3,D,(S64)B,Want,0,0,0);
  if(N!=(S64)Want){Call(6,D,0,0,0,0,0);return N<0?N:-5;}
  for(U64 I=0;I<Want;I++){H^=(unsigned char)B[I];H*=0x100000001b3ULL;}Total+=Want;
 }
 Call(6,D,0,0,0,0,0);Put(F,Path);Put(F," bytes=");Num(F,Total);Put(F," fnv=");Num(F,H);Put(F,"\n");
 return H==Expected?0:-5;
}
__attribute__((noreturn)) void start(void){
 S64 F=Call(5,(S64)"/dev/console",2,0,0,0,0),R;
 for(U64 I=0;I<3;I++)Call(90,F,I,0,0,0,0);
 Put(F,"\nSONY_APFS_BOOTSTRAP_S4J pid=");Num(F,Call(20,0,0,0,0,0,0));Put(F," uid=");Num(F,Call(24,0,0,0,0,0,0));Put(F,"\n");
 R=Call(5,(S64)"/dev/ufs_system",0,0,0,0,0);
 if(R<0){Log(F,"SONY_APFS_STORAGE_ABSENT errno=",-R);Stop(F);}Call(6,R,0,0,0,0,0);
 R=Call(12,(S64)"/dev",0,0,0,0,0);
 Log(F,"SONY_APFS_CHDIR_DEV result=",R);if(R)Stop(F);
 R=Call(136,(S64)"xzpios-root",0755,0,0,0,0);
 Log(F,"SONY_APFS_MKDIR result=",R);if(R)Stop(F);
 R=Call(167,(S64)"apfs",(S64)"/dev/xzpios-root",1,(S64)"/dev/ufs_system",0,0);
 Log(F,"SONY_APFS_MOUNT_SYSTEM result=",R);if(R)Stop(F);
 R=Proof(F,"/dev/xzpios-root/sbin/launchd.ios",11445466571243867227ULL,525088ULL);
 Log(F,"SONY_APFS_APPLE_LAUNCHD_PROOF result=",R);if(R)Stop(F);
 const char *Name="kern.redwood_switch_root",*Value="/dev/xzpios-root System/Volumes/iOS";
 R=Call(274,(S64)Name,Len(Name),0,0,(S64)Value,Len(Value));
 Log(F,"SONY_APFS_SWITCH_ROOT result=",R);if(R)Stop(F);
 R=Call(12,(S64)"/",0,0,0,0,0);if(R){Log(F,"SONY_APFS_CHDIR result=",R);Stop(F);}
 R=Call(167,(S64)"apfs",(S64)"/redwood",1,(S64)"/dev/ufs_tools",0,0);
 Log(F,"SONY_APFS_MOUNT_TOOLS result=",R);if(R)Stop(F);
 R=Proof(F,"/redwood/sbin/init",3619388411429047260ULL,52256ULL);
 Log(F,"SONY_APFS_INIT_PROOF result=",R);if(R)Stop(F);
 char *Args[]={"/redwood/sbin/init",0};char *Env[]={"PATH=/redwood/bin:/redwood/sbin:/bin:/sbin:/usr/bin:/usr/sbin","HOME=/var/root","TERM=dumb",0};
 Put(F,"SONY_APFS_EXEC_INIT\n");R=Call(59,(S64)Args[0],(S64)Args,(S64)Env,0,0,0);Log(F,"SONY_APFS_EXEC_INIT_FAILED result=",R);Stop(F);
}
