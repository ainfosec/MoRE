#ifndef __HYPERVISOR_LOADER_H
#define __HYPERVISOR_LOADER_H

#include "ntddk.h"
#include "..\stdint.h"

// SC ---------------------------------------------------------------------------------------------------------------------------------------

#define IA32_VMX_BASIC_MSR_CODE			0x480
#define IA32_VMX_EPT_VPID_CAP           0x48C
#define IA32_FEATURE_CONTROL_CODE		0x03A
#define IA32_VMX_PROCBASED_CTLS         0x482
#define IA32_VMX_PROCBASED_CTLS2        0x48B

////////////////////
//                //
//  VMX FEATURES  //
//                //
////////////////////
typedef struct _VMX_FEATURES
{
	unsigned SSE3		:1;		// SSE3 Extensions
	unsigned RES1		:2;
	unsigned MONITOR	:1;		// MONITOR/WAIT
	unsigned DS_CPL		:1;		// CPL qualified Debug Store
	unsigned VMX		:1;		// Virtual Machine Techno//Logy
	unsigned RES2		:1;
	unsigned EST		:1;		// Enhanced Intel© Speedstep Techno//Logy
	unsigned TM2		:1;		// Thermal monitor 2
	unsigned SSSE3		:1;		// SSSE3 extensions
	unsigned CID		:1;		// L1 context ID
	unsigned RES3		:2;
	unsigned CX16		:1;		// CMPXCHG16B
	unsigned xTPR		:1;		// Update control
	unsigned PDCM		:1;		// Performance/Debug capability MSR
	unsigned RES4		:2;
	unsigned DCA		:1;
	unsigned RES5		:13;

} VMX_FEATURES;

//////////////
//          //
//  EFLAGS  //
//          //
//////////////
typedef struct _EFLAGS
{
	unsigned Reserved1	:10;
	unsigned ID			:1;		// Identification flag
	unsigned VIP		:1;		// Virtual interrupt pending
	unsigned VIF		:1;		// Virtual interrupt flag
	unsigned AC			:1;		// Alignment check
	unsigned VM			:1;		// Virtual 8086 mode
	unsigned RF			:1;		// Resume flag
	unsigned Reserved2	:1;
	unsigned NT			:1;		// Nested task flag
	unsigned IOPL		:2;		// I/O privilege level
	unsigned OF			:1;
	unsigned DF			:1;
	unsigned IF			:1;		// Interrupt flag
	unsigned TF			:1;		// Task flag
	unsigned SF			:1;		// Sign flag
	unsigned ZF			:1;		// Zero flag
	unsigned Reserved3	:1;
	unsigned AF			:1;		// Borrow flag
	unsigned Reserved4	:1;
	unsigned PF			:1;		// Parity flag
	unsigned Reserved5	:1;
	unsigned CF			:1;		// Carry flag [Bit 0]

} EFLAGS;

///////////
//       //
//  MSR  //
//       //
///////////
typedef struct _MSR
{
	ULONG  Hi;
	ULONG  Lo;
} MSR;

/////////////////////////////
//                         //
//  SPECIAL MSR REGISTERS  //
//                         //
/////////////////////////////
typedef struct _IA32_VMX_BASIC_MSR
{

	unsigned RevId			:32;	// Bits 31...0 contain the VMCS revision identifier
	unsigned szVmxOnRegion  :12;	// Bits 43...32 report # of bytes for VMXON region
	unsigned RegionClear	:1;		// Bit 44 set only if bits 32-43 are clear
	unsigned Reserved1		:3;		// Undefined
	unsigned PhyAddrWidth	:1;		// Physical address width for referencing VMXON, VMCS, etc.
	unsigned DualMon		:1;		// Reports whether the processor supports dual-monitor
									// treatment of SMI and SMM
	unsigned MemType		:4;		// Memory type that the processor uses to access the VMCS
	unsigned VmExitReport	:1;		// Reports weather the procesor reports info in the VM-exit
									// instruction information field on VM exits due to execution
									// of the INS and OUTS instructions
	unsigned Reserved2		:9;		// Undefined

} IA32_VMX_BASIC_MSR;


typedef struct _IA32_FEATURE_CONTROL_MSR
{
	unsigned Lock			:1;		// Bit 0 is the lock bit - cannot be modified once lock is set
	unsigned Reserved1		:1;		// Undefined
	unsigned EnableVmxon	:1;		// Bit 2. If this bit is clear, VMXON causes a general protection exception
	unsigned Reserved2		:29;	// Undefined
	unsigned Reserved3		:32;	// Undefined

} IA32_FEATURE_CONTROL_MSR;

// MoRE

typedef struct _IA32_VMX_EPT_VPID_CAP_MSR
{
	unsigned ExecuteOnly	:1;		// Bit 0 defines if the EPT implementation supports execute-only translation
	unsigned Reserved1		:31;	// Undefined
	unsigned Reserved2		:8;	// Undefined
    unsigned IndividualAddressInvVpid   :1; // Bit 40 defines if type 0 INVVPID instructions are supported
    unsigned Reserved3      :23;

} IA32_VMX_EPT_VPID_CAP_MSR;

typedef struct _IA32_VMX_PROCBASED_CTLS_MSR
{
	unsigned Reserved1		            :32;	// Undefined
	unsigned Reserved2	            	:31;	// Undefined
	unsigned ActivateSecondaryControls	:1;		// Does VMX_PROCBASED_CTLS2_MSR exist

} IA32_VMX_PROCBASED_CTLS_MSR;

typedef struct _IA32_VMX_PROCBASED_CTLS2_MSR
{
	unsigned Reserved1		:32;	// Undefined
	unsigned Reserved2      :1;     // Undefined
	unsigned EnableEpt      :1;     // Whether EPT is enabled/supported
	unsigned Reserved3      :3;      // Undefined
	unsigned EnableVpid     :1;     // Whether VPID is enabled/supported
	unsigned Reserved4		:26;	// Undefined

} IA32_VMX_PROCBASED_CTLS2_MSR;

// End MoRE

/////////////////////////
//                     //
//  CONTROL REGISTERS  //
//                     //
/////////////////////////
typedef struct _CR0_REG
{
	unsigned PE			:1;			// Protected Mode Enabled [Bit 0]
	unsigned MP			:1;			// Monitor Coprocessor FLAG
	unsigned EM			:1;			// Emulate FLAG
	unsigned TS			:1;			// Task Switched FLAG
	unsigned ET			:1;			// Extension Type FLAG
	unsigned NE			:1;			// Numeric Error
	unsigned Reserved1	:10;		//
	unsigned WP			:1;			// Write Protect
	unsigned Reserved2	:1;			//
	unsigned AM			:1;			// Alignment Mask
	unsigned Reserved3	:10;		//
	unsigned NW			:1;			// Not Write-Through
	unsigned CD			:1;			// Cache Disable
	unsigned PG			:1;			// Paging Enabled

} CR0_REG;

typedef struct _CR4_REG
{
	unsigned VME		:1;			// Virtual Mode Extensions
	unsigned PVI		:1;			// Protected-Mode Virtual Interrupts
	unsigned TSD		:1;			// Time Stamp Disable
	unsigned DE			:1;			// Debugging Extensions
	unsigned PSE		:1;			// Page Size Extensions
	unsigned PAE		:1;			// Physical Address Extension
	unsigned MCE		:1;			// Machine-Check Enable
	unsigned PGE		:1;			// Page Global Enable
	unsigned PCE		:1;			// Performance-Monitoring Counter Enable
	unsigned OSFXSR		:1;			// OS Support for FXSAVE/FXRSTOR
	unsigned OSXMMEXCPT	:1;			// OS Support for Unmasked SIMD Floating-Point Exceptions
	unsigned Reserved1	:2;			//
	unsigned VMXE		:1;			// Virtual Machine Extensions Enabled
	unsigned Reserved2	:18;		//

} CR4_REG;

/////////////////
//             //
//  MISC DATA  //
//             //
/////////////////
typedef struct _MISC_DATA
{
	unsigned	Reserved1		:6;		// [0-5]
	unsigned	ActivityStates	:3;		// [6-8]
	unsigned	Reserved2		:7;		// [9-15]
	unsigned	CR3Targets		:9;		// [16-24]

	// 512*(N+1) is the recommended maximum number of MSRs
	unsigned	MaxMSRs			:3;		// [25-27]

	unsigned	Reserved3		:4;		// [28-31]
	unsigned	MSEGRevID		:32;	// [32-63]

} MISC_DATA;

//////////////////////////
//                      //
//  SELECTOR REGISTERS  //
//                      //
//////////////////////////
typedef struct _GDTR
{
	unsigned	Limit		:16;
	unsigned	BaseLo		:16;
	unsigned	BaseHi		:16;

} GDTR;

typedef struct _IDTR
{
	unsigned	Limit		:16;
	unsigned	BaseLo		:16;
	unsigned	BaseHi		:16;

} IDTR;

typedef struct	_SEG_DESCRIPTOR
{
	unsigned	LimitLo	:16;
	unsigned	BaseLo	:16;
	unsigned	BaseMid	:8;
	unsigned	Type	:4;
	unsigned	System	:1;
	unsigned	DPL		:2;
	unsigned	Present	:1;
	unsigned	LimitHi	:4;
	unsigned	AVL		:1;
	unsigned	L		:1;
	unsigned	DB		:1;
	unsigned	Gran	:1;		// Granularity
	unsigned	BaseHi	:8;

} SEG_DESCRIPTOR;

VOID ReadMSR( ULONG msrEncoding );
VOID Beep( ULONG state );

#endif
