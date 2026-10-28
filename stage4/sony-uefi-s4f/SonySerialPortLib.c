// SPDX-License-Identifier: MIT
#include <Base.h>
#include <Library/SerialPortLib.h>

#define CONSOLE_BASE 0xffcc0000UL
#define CONSOLE_CAPACITY (0x40000U - 12U)
#define CONSOLE_SIGNATURE 0x43474244U

// The physical range is also identity mapped after MemoryPeim enables the MMU.
STATIC VOID Flush (UINTN Begin, UINTN End) {
  UINT64 Ctr;
  UINTN Line, Address;
  __asm__ volatile ("mrs %0, ctr_el0" : "=r" (Ctr));
  Line = 4U << ((Ctr >> 16) & 15U);
  for (Address = Begin & ~(Line - 1); Address < End; Address += Line) {
    __asm__ volatile ("dc cvac, %0" :: "r" (Address) : "memory");
  }
  __asm__ volatile ("dsb sy" ::: "memory");
}

RETURN_STATUS EFIAPI SerialPortInitialize (VOID) {
  volatile UINT32 *Header = (volatile UINT32 *)CONSOLE_BASE;
  if (Header[0] != CONSOLE_SIGNATURE || Header[1] > CONSOLE_CAPACITY || Header[2] != Header[1]) {
    Header[1] = Header[2] = 0;
    Header[0] = CONSOLE_SIGNATURE;
    Flush (CONSOLE_BASE, CONSOLE_BASE + 12);
  }
  return RETURN_SUCCESS;
}

UINTN EFIAPI SerialPortWrite (IN UINT8 *Buffer, IN UINTN NumberOfBytes) {
  volatile UINT32 *Header = (volatile UINT32 *)CONSOLE_BASE;
  volatile UINT8 *Data = (volatile UINT8 *)(CONSOLE_BASE + 12);
  UINTN Count, Start, Index;
  SerialPortInitialize ();
  Start = Header[2];
  Count = NumberOfBytes;
  if (Count > CONSOLE_CAPACITY - Start) Count = CONSOLE_CAPACITY - Start;
  for (Index = 0; Index < Count; Index++) Data[Start + Index] = Buffer[Index];
  Flush (CONSOLE_BASE + 12 + Start, CONSOLE_BASE + 12 + Start + Count);
  Header[1] = Header[2] = (UINT32)(Start + Count);
  Flush (CONSOLE_BASE, CONSOLE_BASE + 12);
  return NumberOfBytes;
}

UINTN EFIAPI SerialPortRead (OUT UINT8 *Buffer, IN UINTN NumberOfBytes) { return 0; }
BOOLEAN EFIAPI SerialPortPoll (VOID) { return FALSE; }
RETURN_STATUS EFIAPI SerialPortSetControl (IN UINT32 Control) { return RETURN_UNSUPPORTED; }
RETURN_STATUS EFIAPI SerialPortGetControl (OUT UINT32 *Control) {
  *Control = EFI_SERIAL_INPUT_BUFFER_EMPTY | EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
  return RETURN_SUCCESS;
}
RETURN_STATUS EFIAPI SerialPortSetAttributes (IN OUT UINT64 *BaudRate, IN OUT UINT32 *ReceiveFifoDepth,
  IN OUT UINT32 *Timeout, IN OUT EFI_PARITY_TYPE *Parity, IN OUT UINT8 *DataBits,
  IN OUT EFI_STOP_BITS_TYPE *StopBits) { return RETURN_UNSUPPORTED; }
