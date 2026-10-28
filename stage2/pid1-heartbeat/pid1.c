// SPDX-License-Identifier: MIT
// Freestanding Darwin arm64 PID1: console output and one-second select heartbeats.
typedef unsigned long U64;
typedef long S64;
static S64 Call(S64 Number,S64 A,S64 B,S64 C,S64 D,S64 E) {
  register S64 X16 __asm__("x16")=Number;
  register S64 X0 __asm__("x0")=A;
  register S64 X1 __asm__("x1")=B;
  register S64 X2 __asm__("x2")=C;
  register S64 X3 __asm__("x3")=D;
  register S64 X4 __asm__("x4")=E;
  __asm__ volatile("svc #0x80":"+r"(X0),"+r"(X1),"+r"(X2),"+r"(X3),"+r"(X4),"+r"(X16)::"memory","cc");
  return X0;
}
static void Put(S64 Fd,const char *Text) {
  U64 Length=0;while(Text[Length])Length++;
  Call(4,Fd,(S64)Text,(S64)Length,0,0);
}
static void Number(S64 Fd,U64 Value) {
  char Buffer[24];U64 I=sizeof(Buffer)-1;Buffer[I]=0;
  do { Buffer[--I]='0'+Value%10;Value/=10; } while(Value);
  Put(Fd,Buffer+I);
}
__attribute__((noreturn)) void start(void) {
  S64 Fd=Call(5,(S64)"/dev/console",2,0,0,0);
  U64 Beat=0;
  Put(Fd,"\nXZPIOS PID1 START pid=");Number(Fd,(U64)Call(20,0,0,0,0,0));
  Put(Fd," uid=");Number(Fd,(U64)Call(24,0,0,0,0,0));Put(Fd,"\n");
  for (;;) {
    struct { S64 Seconds;int Microseconds,Padding; } Timeout={1,0,0};
    Call(93,0,0,0,0,(S64)&Timeout);
    if (Beat<16) {
      Put(Fd,"XZPIOS PID1 HEARTBEAT ");Number(Fd,++Beat);Put(Fd,"\n");
    }
  }
}
