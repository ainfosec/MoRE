/*
	Virtual Machine Monitor
    Copyright (C) 2007  Shawn Embleton
    Copyright (C) 2012  Assured Information Security
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ntifs.h"
#include "ntddk.h"

// SC ---------------------------------------------------------------------------------------------------------------------------------------
//
// Add the Hypervisor's additional modules.

#include "hypervisor_loader.h"
#include "hypervisor.h"
#include "procmon.h"
#include "..\paging.h"
#include "ept.h"

//////////////////
//              //
//  PROTOTYPES  //
//              //
//////////////////
NTSTATUS	DriverEntry( IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath );
VOID		DriverUnload( IN PDRIVER_OBJECT DriverObject );
VOID		StartVMX( );

///////////////
//           //
//  Globals  //
//           //
///////////////
ULONG			*pVMXONRegion		= NULL;		// Memory address of VMXON region.
ULONG			*pVMCSRegion		= NULL;		// Memory address of VMCS region.
ULONG			VMXONRegionSize		= 0;		// Size of of VMXON region.
ULONG			VMCSRegionSize		= 0;		// Size of of VMCS region.
ULONG			ErrorCode		= 0;

EFLAGS						eFlags			= {0};
MSR						msr			= {0};

PVOID						FakeStack		= NULL;

ULONG						ScrubTheLaunch		= 0;

PHYSICAL_ADDRESS				PhysicalVMXONRegionPtr = {0};
PHYSICAL_ADDRESS				PhysicalVMCSRegionPtr = {0};

VMX_FEATURES					vmxFeatures;
IA32_VMX_BASIC_MSR				vmxBasicMsr;
IA32_FEATURE_CONTROL_MSR		vmxFeatureControl;
// MoRE
IA32_VMX_EPT_VPID_CAP_MSR       vmxEptMsr;
IA32_VMX_PROCBASED_CTLS_MSR     vmxProcCtls;
IA32_VMX_PROCBASED_CTLS2_MSR    vmxProcCtls2;
EptPml4Entry                    *EptPml4TablePointer = NULL; // Pointer to the EPT PML4 table
// End MoRE

CR0_REG						cr0_reg = {0};
CR4_REG						cr4_reg = {0};

ULONG						temp32 = 0;
USHORT						temp16 = 0;

GDTR						gdt_reg = {0};
IDTR						idt_reg = {0};

ULONG						gdt_base = 0;
ULONG						idt_base = 0;

USHORT						mLDT = 0;
USHORT						seg_selector = 0;

SEG_DESCRIPTOR				segDescriptor = {0};
MISC_DATA					misc_data = {0};

PVOID						GuestReturn = NULL;
ULONG						GuestStack = 0;

///////////////
//           //
//  SET BIT  //
//           //
///////////////
VOID SetBit( ULONG * dword, ULONG bit )
{
	ULONG mask = ( 1 << bit );
	*dword = *dword | mask;
}

/////////////////
//             //
//  CLEAR BIT  //
//             //
/////////////////
VOID ClearBit( ULONG * dword, ULONG bit )
{
	ULONG mask = 0xFFFFFFFF;
	ULONG sub = ( 1 << bit );
	mask = mask - sub;
	*dword = *dword & mask;
}

//	Loads the contents of a 64-bit model specific register (MSR) specified
//	in the ECX register into registers EDX:EAX. The EDX register is loaded
//	with the high-order 32 bits of the MSR and the EAX register is loaded
//	with the low-order 32 bits.
//		msr.Hi --> EDX
//		msr.Lo --> EAX
//
VOID ReadMSR( ULONG msrEncoding )
{
	__asm
	{
		PUSHAD

		MOV		ECX, msrEncoding

		RDMSR

		MOV		msr.Hi, EDX
		MOV		msr.Lo, EAX

		POPAD
	}
}

//	Writes the contents of registers EDX:EAX into the 64-bit model specific
//	register (MSR) specified in the ECX register. The contents of the EDX
//	register are copied to high-order 32 bits of the selected MSR and the
//	contents of the EAX register are copied to low-order 32 bits of the MSR.
//		msr.Hi <-- EDX
//		msr.Lo <-- EAX
//
VOID WriteMSR( ULONG msrEncoding )
{
	__asm
	{
		PUSHAD

		MOV		EDX, msr.Hi
		MOV		EAX, msr.Lo
		MOV		ECX, msrEncoding

		WRMSR

		POPAD
	}
}

////////////////////////////////////
//                                //
//  SEGMENT DESCRIPTOR OPERATORS  //
//                                //
////////////////////////////////////
ULONG GetSegmentDescriptorBase( ULONG gdt_base , USHORT seg_selector )
{
	ULONG			base = 0;
	SEG_DESCRIPTOR	segDescriptor = {0};

	RtlCopyBytes( &segDescriptor, (ULONG *)(gdt_base + (seg_selector >> 3) * 8), 8 );
	base = segDescriptor.BaseHi;
	base <<= 8;
	base |= segDescriptor.BaseMid;
	base <<= 16;
	base |= segDescriptor.BaseLo;

	return base;
}

ULONG GetSegmentDescriptorDPL( ULONG gdt_base , USHORT seg_selector )
{
	SEG_DESCRIPTOR	segDescriptor = {0};

	RtlCopyBytes( &segDescriptor, (ULONG *)(gdt_base + (seg_selector >> 3) * 8), 8 );

	return segDescriptor.DPL;
}

ULONG GetSegmentDescriptorLimit( ULONG gdt_base , USHORT seg_selector )
{
	SEG_DESCRIPTOR	segDescriptor = {0};

	RtlCopyBytes( &segDescriptor, (ULONG *)(gdt_base + (seg_selector >> 3) * 8), 8 );

	return ( (segDescriptor.LimitHi << 16) | segDescriptor.LimitLo );
}

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// SC has some additional helper functions the are not originally provided.

////////////
//        //
//  BEEP  //
//        //
////////////
VOID Beep( ULONG state )
{
	if (state == 1)
	{
		__asm
		{
			PUSHAD

			mov al, 0xFF
			out 0x61, al

			POPAD
		}
	}
	else
	{
		__asm
		{
			PUSHAD

			mov al, 0x00
			out 0x61, al

			POPAD
		}
	}
}

///////////////
//           //
//  SET BIT  //
//           //
///////////////
VOID SetBit16( USHORT * dword, ULONG bit )
{
	USHORT mask = ( 1 << bit );
	*dword = *dword | mask;
}

VOID SetBit32( ULONG * dword, ULONG bit )
{
	ULONG mask = ( 1 << bit );
	*dword = *dword | mask;
}

/////////////////
//             //
//  CLEAR BIT  //
//             //
/////////////////
VOID ClearBit16( USHORT * dword, ULONG bit )
{
	USHORT mask = 0xFFFF;
	USHORT sub = ( 1 << bit );
	mask = mask - sub;
	*dword = *dword & mask;
}

VOID ClearBit32( ULONG * dword, ULONG bit )
{
	ULONG mask = 0xFFFFFFFF;
	ULONG sub = ( 1 << bit );
	mask = mask - sub;
	*dword = *dword & mask;
}

/////////////////////////
//                     //
//  SET BIT IO Bitmap  //
//                     //
/////////////////////////
VOID SetBit_IOBitmap( ULONG * IOBitmap, ULONG bit )
{
	ULONG count = 0;
	ULONG index = bit;

	// Loop until the index is less than 32 bit (set bit works on 32 bit words).
	while (index > 32)
	{
		count++;
		index -= 32;
	}

	// Store the current value.
	temp32 = *(IOBitmap + count);

	// Set the correct bit.
	SetBit32(&temp32, index);

	// Store the new value.
	*(IOBitmap + count) = temp32;
}

// SC ---------------------------------------------------------------------------------------------------------------------------------------

///////////
//       //
//  VMX  //
//       //
///////////
__declspec( naked ) VOID StartVMX( )
{
	//
	//	Get the Guest Return EIP.
	//
	//
	//	Hi	|		|
	//		+---------------+
	//		|      EIP	|
	//		+---------------+ <--	ESP after the CALL
	//	Lo	|		|
	//
	//

	__asm	POP	GuestReturn

//	//Log("Guest Return EIP" , GuestReturn );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// We do not support multi-core.

	///////////////////////////
	//                       //
	//  SET THREAD AFFINITY  //
	//                       //
	///////////////////////////

//	//Log( "Enabling VMX mode on CPU 0", 0 );
//	KeSetSystemAffinityThread( (KAFFINITY) 0x00000001 );
//	//Log( "Running on Processor" , KeGetCurrentProcessorNumber() );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	////////////////
	//            //
	//  GDT Info  //
	//            //
	////////////////
	__asm
	{
		SGDT	gdt_reg
	}

	temp32 = 0;
	temp32 = gdt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= gdt_reg.BaseLo;
	gdt_base = temp32;
//	//Log( "GDT Base", gdt_base );
//	//Log( "GDT Limit", gdt_reg.Limit );

	////////////////////////////
	//                        //
	//  IDT Segment Selector  //
	//                        //
	////////////////////////////
	__asm	SIDT	idt_reg

	temp32 = 0;
	temp32 = idt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= idt_reg.BaseLo;
	idt_base = temp32;
//	//Log( "IDT Base", idt_base );
//	//Log( "IDT Limit", idt_reg.Limit );

	//	(1)	Check VMX support in processor using CPUID.
	__asm
	{
		PUSHAD

		MOV		EAX, 1
		CPUID

		// ECX contains the VMX_FEATURES FLAGS (VMX supported if bit 5 equals 1)
		MOV		vmxFeatures, ECX

		MOV		EAX, 0x80000008
		CPUID
		MOV		temp32, EAX

		POPAD
	}

	if( vmxFeatures.VMX == 0 )
	{
//		//Log( "VMX Support Not Present." , vmxFeatures );
		goto Abort;
	}

	Log( "VMX Support Present." , vmxFeatures );

	//	(2)	Determine the VMX capabilities supported by the processor through
	//		the VMX capability MSRs.
	__asm
	{
		PUSHAD

		MOV		ECX, IA32_VMX_BASIC_MSR_CODE
		RDMSR
		LEA		EBX, vmxBasicMsr
		MOV		[EBX+4], EDX
		MOV		[EBX], EAX

		MOV		ECX, IA32_FEATURE_CONTROL_CODE
		RDMSR
		LEA		EBX, vmxFeatureControl
		MOV		[EBX+4], EDX
		MOV		[EBX], EAX
// MoRE		
		MOV		ECX, IA32_VMX_PROCBASED_CTLS
		RDMSR
		LEA		EBX, vmxProcCtls
		MOV		[EBX+4], EDX
		MOV		[EBX], EAX

		POPAD
	};
	
	if (vmxProcCtls.ActivateSecondaryControls != 1)
	{
        Log("ERROR: No secondary support!", 0);
        goto Abort;
    }
    // Does this system support EPT/VPID?
    __asm
	{
		PUSHAD
		
		MOV		ECX, IA32_VMX_PROCBASED_CTLS2
		RDMSR
		LEA		EBX, vmxProcCtls2
		MOV		[EBX+4], EDX
		MOV		[EBX], EAX

		POPAD
	};
	
	if (vmxProcCtls2.EnableEpt == 0 && vmxProcCtls2.EnableVpid == 0)
	{
        Log("ERROR: No VPID / EPT support!", 0);
        goto Abort;
    }
    
    // Get the EPT/VPID capabilities for the system
    __asm
	{
		PUSHAD
		MOV		ECX, IA32_VMX_EPT_VPID_CAP
		RDMSR
		LEA		EBX, vmxEptMsr
		MOV		[EBX+4], EDX
		MOV		[EBX], EAX

		POPAD
	};
	
	if (vmxEptMsr.ExecuteOnly != 1)
	{
        Log("ERROR: Execute-only EPT translations not supported!", 0);
        goto Abort;
    } 
    else
    {
        Log("Execute-only EPT is supported", 0);
    }
    ProcessorSupportsType0InvVpid = (uint8) vmxEptMsr.IndividualAddressInvVpid;
    Log("Processor support for individual address INVVPID", ProcessorSupportsType0InvVpid);
// End MoRE

	//	(3)	Create a VMXON region in non-pageable memory of a size specified by
	//		IA32_VMX_BASIC_MSR and aligned to a 4-byte boundary. The VMXON region
	//		must be hosted in cache-coherent memory.
//	//Log( "VMXON Region Size" , vmxBasicMsr.szVmxOnRegion ) ;
//	//Log( "VMXON Access Width Bit" , vmxBasicMsr.PhyAddrWidth );
//	//Log( "      [   1] --> 32-bit" , 0 );
//	//Log( "      [   0] --> 64-bit" , 0 );
//	//Log( "VMXON Memory Type", vmxBasicMsr.MemType );
//	//Log( "      [   0]  --> Strong Uncacheable" , 0 );
//	//Log( "      [ 1-5]  --> Unused" , 0 );
//	//Log( "      [   6]  --> Write Back" , 0 );
//	//Log( "      [7-15]  --> Unused" , 0 );

	VMXONRegionSize = vmxBasicMsr.szVmxOnRegion;

	switch( vmxBasicMsr.MemType )
	{
		case 0:
			//Log( "Unsupported memory type." , vmxBasicMsr.MemType );
			goto Abort;
			break;
		case 6:
			break;
		default:
			//Log( "ERROR : Unknown VMXON Region memory type." , 0);
			goto Abort;
			break;
	}

	//	(4)	Initialize the version identifier in the VMXON region (first 32 bits)
	//		with the VMCS revision identifier reported by capability MSRs.
	*(pVMXONRegion) = vmxBasicMsr.RevId;

//	//Log( "vmxBasicMsr.RevId" , vmxBasicMsr.RevId );

	//	(5)	Ensure the current processor operating mode meets the required CR0
	//		fixed bits (CR0.PE=1, CR0.PG=1). Other required CR0 fixed bits can
	//		be detected through the IA32_VMX_CR0_FIXED0 and IA32_VMX_CR0_FIXED1
	//		MSRs.
	__asm
	{
		PUSH	EAX

		MOV		EAX, CR0
		MOV		cr0_reg, EAX

		POP		EAX
	}
	if( cr0_reg.PE != 1 )
	{
		//Log( "ERROR : Protected Mode not enabled." , 0 );
		//Log( "Value of CR0" , cr0_reg );
		goto Abort;
	}

//	//Log( "Protected Mode enabled." , 0 );

	if( cr0_reg.PG != 1 )
	{
		//Log( "ERROR : Paging not enabled." , 0 );
		//Log( "Value of CR0" , cr0_reg );
		goto Abort;
	}

//	//Log( "Paging enabled." , 0 );

	cr0_reg.NE = 1;

	__asm
	{
		PUSH	EAX

		MOV		EAX, cr0_reg
		MOV		CR0, EAX

		POP		EAX
	}

	//	(6)	Enable VMX operation by setting CR4.VMXE=1 [bit 13]. Ensure the
	//		resultant CR4 value supports all the CR4 fixed bits reported in
	//		the IA32_VMX_CR4_FIXED0 and IA32_VMX_CR4_FIXED1 MSRs.
	__asm
	{
		PUSH	EAX

		_emit	0x0F	// MOV	EAX, CR4
		_emit	0x20
		_emit	0xE0

		MOV		cr4_reg, EAX

		POP		EAX
	}

//	//Log( "CR4" , cr4_reg );
	cr4_reg.VMXE = 1;
//	//Log( "CR4" , cr4_reg );

	__asm
	{
		PUSH	EAX

		MOV		EAX, cr4_reg

		_emit	0x0F	// MOV	CR4, EAX
		_emit	0x22
		_emit	0xE0

		POP		EAX
	}

	//	(7)	Ensure that the IA32_FEATURE_CONTROL_MSR (MSR index 0x3A) has been
	//		properly programmed and that its lock bit is set (bit 0=1). This MSR
	//		is generally configured by the BIOS using WRMSR.
//	//Log( "IA32_FEATURE_CONTROL Lock Bit" , vmxFeatureControl.Lock );
	if( vmxFeatureControl.Lock != 1 )
	{
		//Log( "ERROR : Feature Control Lock Bit != 1." , 0 );
		goto Abort;
	}

	//	(8)	Execute VMXON with the physical address of the VMXON region as the
	//		operand. Check successful execution of VMXON by checking if
	//		RFLAGS.CF=0.
	__asm
	{
		PUSH	DWORD PTR 0
		PUSH	DWORD PTR PhysicalVMXONRegionPtr.LowPart

		_emit	0xF3	// VMXON [ESP]
		_emit	0x0F
		_emit	0xC7
		_emit	0x34
		_emit	0x24

		PUSHFD
		POP		eFlags

		ADD		ESP, 8
	}
    
// MoRE
    // As per the Intel spec, this will prevent previously cached translations to interfere
    InvVpidAllContext();
// End MoRE
    
	if( eFlags.CF == 1 )
	{
		//Log( "ERROR : VMXON operation failed." , 0 );
		goto Abort;
	}

//	//Log( "SUCCESS : VMXON operation completed." , 0 );
//	//Log( "VMM is now running." , 0 );

	//
	//	***	The processor is now in VMX root operation!
	//

	//	(1)	Create a VMCS region in non-pageable memory of size specified by
	//		the VMX capability MSR IA32_VMX_BASIC and aligned to 4-KBytes.
	//		Software should read the capability MSRs to determine width of the
	//		physical addresses that may be used for a VMCS region and ensure
	//		the entire VMCS region can be addressed by addresses with that width.
	//		The term "guest-VMCS address" refers to the physical address of the
	//		new VMCS region for the following steps.
	VMCSRegionSize = vmxBasicMsr.szVmxOnRegion;

	switch( vmxBasicMsr.MemType )
	{
		case 0:
			//Log( "Unsupported memory type." , vmxBasicMsr.MemType );
			goto Abort;
			break;
		case 6:
			break;
		default:
			//Log( "ERROR : Unknown VMCS Region memory type." , 0 );
			goto Abort;
			break;
	}

	//	(2)	Initialize the version identifier in the VMCS (first 32 bits)
	//		with the VMCS revision identifier reported by the VMX
	//		capability MSR IA32_VMX_BASIC.
	*(pVMCSRegion) = vmxBasicMsr.RevId;

	//	(3)	Execute the VMCLEAR instruction by supplying the guest-VMCS address.
	//		This will initialize the new VMCS region in memory and set the launch
	//		state of the VMCS to "clear". This action also invalidates the
	//		working-VMCS pointer register to FFFFFFFF_FFFFFFFFH. Software should
	//		verify successful execution of VMCLEAR by checking if RFLAGS.CF = 0
	//		and RFLAGS.ZF = 0.
	__asm
	{
		PUSH	DWORD PTR 0
		PUSH	DWORD PTR PhysicalVMCSRegionPtr.LowPart

		_emit	0x66	// VMCLEAR [ESP]
		_emit	0x0F
		_emit	0xc7
		_emit	0x34
		_emit	0x24

		ADD		ESP, 8

		PUSHFD
		POP		eFlags
	}
	if( eFlags.CF != 0 || eFlags.ZF != 0 )
	{
		//Log( "ERROR : VMCLEAR operation failed." , 0 );
		goto Abort;
	}

//	//Log( "SUCCESS : VMCLEAR operation completed." , 0 );

	//	(4)	Execute the VMPTRLD instruction by supplying the guest-VMCS address.
	//		This initializes the working-VMCS pointer with the new VMCS region’s
	//		physical address.
	__asm
	{
		PUSH	DWORD PTR 0
		PUSH	DWORD PTR PhysicalVMCSRegionPtr.LowPart

		_emit	0x0F	// VMPTRLD [ESP]
		_emit	0xC7
		_emit	0x34
		_emit	0x24

		ADD		ESP, 8
	}

	//
	//  ***************************************
	//  *                                     *
	//  *	H.1.1 16-Bit Guest-State Fields   *
	//  *                                     *
	//  ***************************************
    
	//			Guest ES selector									00000800H
				__asm	MOV		seg_selector, ES
//				//Log( "Setting Guest ES Selector" , seg_selector );
				WriteVMCS( 0x00000800, seg_selector );

	//			Guest CS selector									00000802H
				__asm	MOV		seg_selector, CS
//				//Log( "Setting Guest CS Selector" , seg_selector );
				WriteVMCS( 0x00000802, seg_selector );

	//			Guest SS selector									00000804H
				__asm	MOV		seg_selector, SS
//				//Log( "Setting Guest SS Selector" , seg_selector );
				WriteVMCS( 0x00000804, seg_selector );

	//			Guest DS selector									00000806H
				__asm	MOV		seg_selector, DS
//				//Log( "Setting Guest DS Selector" , seg_selector );
				WriteVMCS( 0x00000806, seg_selector );

	//			Guest FS selector									00000808H
				__asm	MOV		seg_selector, FS
//				//Log( "Setting Guest FS Selector" , seg_selector );
				WriteVMCS( 0x00000808, seg_selector );

	//			Guest GS selector									0000080AH
				__asm	MOV		seg_selector, GS
//				//Log( "Setting Guest GS Selector" , seg_selector );
				WriteVMCS( 0x0000080A, seg_selector );

	//			Guest TR selector									0000080EH
				__asm	STR		seg_selector

// SC ---------------------------------------------------------------------------------------------------------------------------------------

				//ClearBit( &seg_selector, 2 );						// TI Flag

				ClearBit16( &seg_selector, 2 );						// TI Flag

// SC ---------------------------------------------------------------------------------------------------------------------------------------

//				//Log( "Setting Guest TR Selector" , seg_selector );
				WriteVMCS( 0x0000080E, seg_selector );

	//  **************************************
	//  *                                    *
	//  *	H.1.2 16-Bit Host-State Fields   *
    
	//  *                                    *
	//  **************************************
	//
	//			Host ES selector									00000C00H
				__asm	MOV		seg_selector, ES
				seg_selector &= 0xFFFC;
//				//Log( "Setting Host ES Selector" , seg_selector );
				WriteVMCS( 0x00000C00, seg_selector );

	//			Host CS selector									00000C02H
				__asm	MOV		seg_selector, CS
//				//Log( "Setting Host CS Selector" , seg_selector );
				WriteVMCS( 0x00000C02, seg_selector );

	//			Host SS selector									00000C04H
				__asm	MOV		seg_selector, SS
//				//Log( "Setting Host SS Selector" , seg_selector );
				WriteVMCS( 0x00000C04, seg_selector );

	//			Host DS selector									00000C06H
				__asm	MOV		seg_selector, DS
				seg_selector &= 0xFFFC;
//				//Log( "Setting Host DS Selector" , seg_selector );
				WriteVMCS( 0x00000C06, seg_selector );

	//			Host FS selector									00000C08H
				__asm	MOV		seg_selector, FS
//				//Log( "Setting Host FS Selector" , seg_selector );
				WriteVMCS( 0x00000C08, seg_selector );

	//			Host GS selector									00000C0AH
				__asm	MOV		seg_selector, GS
				seg_selector &= 0xFFFC;
//				//Log( "Setting Host GS Selector" , seg_selector );
				WriteVMCS( 0x00000C0A, seg_selector );

	//			Host TR selector									00000C0CH
				__asm	STR		seg_selector
//				//Log( "Setting Host TR Selector" , seg_selector );
				WriteVMCS( 0x00000C0C, seg_selector );

	//  ***************************************
	//  *                                     *
	//  *	H.2.2 64-Bit Guest-State Fields   *
	//  *                                     *
	//  ***************************************
	//
	//			VMCS Link Pointer (full)							00002800H
				temp32 = 0xFFFFFFFF;
//				//Log( "Setting VMCS Link Pointer (full)" , temp32 );
				WriteVMCS( 0x00002800, temp32 );

	//			VMCS link pointer (high)							00002801H
				temp32 = 0xFFFFFFFF;
//				//Log( "Setting VMCS Link Pointer (high)" , temp32 );
				WriteVMCS( 0x00002801, temp32 );

	//			Reserved Bits of IA32_DEBUGCTL MSR must be 0
	//			(1D9H)
				ReadMSR( 0x000001D9 );
//				//Log( "IA32_DEBUGCTL MSR" , msr.Lo );

	//			Guest IA32_DEBUGCTL (full)							00002802H
				temp32 = msr.Lo;
//				//Log( "Setting Guest IA32_DEBUGCTL (full)" , temp32 );
				WriteVMCS( 0x00002802, temp32 );

	//			Guest IA32_DEBUGCTL (high)							00002803H
				temp32 = msr.Hi;
//				//Log( "Setting Guest IA32_DEBUGCTL (high)" , temp32 );
				WriteVMCS( 0x00002803, temp32 );

	//  ***********************************
	//  *                                 *
	//  *	H.3.1 32-Bit Control Fields   *
	//  *                                 *
	//  ***********************************
	//
	//			Pin-based VM-execution controls						00004000H
	//			IA32_VMX_PINBASED_CTLS MSR (index 481H)
				ReadMSR( 0x481 );
//					//Log( "Pin-based allowed-0" , msr.Lo );
//					//Log( "Pin-based allowed-1" , msr.Hi );
				temp32 = 0;
				temp32 |= msr.Lo;
				temp32 &= msr.Hi;
				//SetBit( &temp32, 3 );
//				//Log( "Setting Pin-Based Controls Mask" , temp32 );
				WriteVMCS( 0x00004000, temp32 );

	//			Primary processor-based VM-execution controls		00004002H
	//			IA32_VMX_PROCBASED_CTLS MSR (index 482H)
				ReadMSR( 0x482 );
//					//Log( "Proc-based allowed-0" , msr.Lo );
//					//Log( "Proc-based allowed-1" , msr.Hi );
				temp32 = 0;
				temp32 |= msr.Lo;
				temp32 &= msr.Hi;
//				//Log( "Setting Pri Proc-Based Controls Mask" , temp32 );
				WriteVMCS( 0x00004002, temp32 );

	//			Exception bitmap									00004004H
				temp32 = 0x00000000;
				//SetBit( &temp32, 14 );							// Page Fault
				//SetBit( &temp32, 3 );							// Software Interrupt (INT 3)
				//SetBit( &temp32, 7 );							// No Math Co-Processor
				//SetBit( &temp32, 8 );							// Doudle Fault
// MoRE
                // Enable exiting on the trap (INT 1)
                SetBit(&temp32, 1);
// End MoRE
//				//Log( "Exception Bitmap" , temp32 );
				WriteVMCS( 0x00004004, temp32 );

	//			Page-fault error-code mask							00004006H
//				//Log( "Page-Fault Error-Code Mask" , 0 );
				WriteVMCS( 0x00004006, 0 );

	//			Page-fault error-code match							00004008H
//				//Log( "Page-Fault Error-Code Match" , 0 );
				WriteVMCS( 0x00004008, 0 );

				//	Get the CR3-target count, MSR store/load counts, et cetera
				//
				//	IA32_VMX_MISC MSR (index 485H)
				ReadMSR( 0x485 );
//				//Log( "Misc Data" , msr.Lo );
				////Log( "Misc Data" , msr.Hi );
				RtlCopyBytes( &misc_data, &msr.Lo, 4 );
//				//Log( "   ActivityStates" , misc_data.ActivityStates );
//				//Log( "   CR3Targets" , misc_data.CR3Targets );
//				//Log( "   MaxMSRs" , misc_data.MaxMSRs );

	//			VM-exit controls									0000400CH
	//			IA32_VMX_EXIT_CTLS MSR (index 483H)
				ReadMSR( 0x483 );
//					//Log( "Exit controls allowed-0" , msr.Lo );
//					//Log( "Exit controls allowed-1" , msr.Hi );
				temp32 = 0;
				temp32 |= msr.Lo;
				temp32 &= msr.Hi;
				SetBit( &temp32, 15 );								// Acknowledge Interrupt On Exit
//				//Log( "Setting VM-Exit Controls Mask" , temp32 );
				WriteVMCS( 0x0000400C, temp32 );

	//			VM-entry controls									00004012H
	//			IA32_VMX_ENTRY_CTLS MSR (index 484H)
				ReadMSR( 0x484 );
//					//Log( "VMX Entry allowed-0" , msr.Lo );
//					//Log( "VMX Entry allowed-1" , msr.Hi );
				temp32 = 0;
				temp32 |= msr.Lo;
				temp32 &= msr.Hi;
				ClearBit( &temp32 , 9 );							// IA-32e Mode Guest Disable
//				//Log( "Setting VM-Entry Controls Mask" , temp32 );
				WriteVMCS( 0x00004012, temp32 );

	//  ***************************************
	//  *                                     *
	//  *	H.3.3 32-Bit Guest-State Fields   *
	//  *                                     *
	//  ***************************************
	//
	//			Guest ES limit										00004800H
				__asm	MOV seg_selector, ES
				temp32 = 0;
				temp32 = GetSegmentDescriptorLimit( gdt_base, seg_selector );
//				//Log( "Setting Guest ES limit" , 0xFFFFFFFF );
				WriteVMCS( 0x00004800, 0xFFFFFFFF );

	//			Guest CS limit										00004802H
				__asm	MOV seg_selector, CS
				temp32 = 0;
				temp32 = GetSegmentDescriptorLimit( gdt_base, seg_selector );
//				//Log( "Setting Guest CS limit" , 0xFFFFFFFF );
				WriteVMCS( 0x00004802, 0xFFFFFFFF );

	//			Guest SS limit										00004804H
				__asm	MOV seg_selector, SS
				temp32 = 0;
				temp32 = GetSegmentDescriptorLimit( gdt_base, seg_selector );
//				//Log( "Setting Guest SS limit" , 0xFFFFFFFF );
				WriteVMCS( 0x00004804, 0xFFFFFFFF );

	//			Guest DS limit										00004806H
				__asm	MOV seg_selector, DS
				temp32 = 0;
				temp32 = GetSegmentDescriptorLimit( gdt_base, seg_selector );
//				//Log( "Setting Guest DS limit" , 0xFFFFFFFF );
				WriteVMCS( 0x00004806, 0xFFFFFFFF );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// The following is the old code. This needed to be changed to add support for
// Windows 7. My guess is this new code is the correct way of doing this and
// the original author, wasimply testing something out and left it in this
// state.

//	//			Guest FS limit										00004808H
//				__asm	MOV seg_selector, FS
//				temp32 = 0;
//				temp32 = GetSegmentDescriptorLimit( gdt_base, seg_selector );
//				//Log( "Setting Guest FS limit" , 0x00001000 );
//				WriteVMCS( 0x00004808, 0x00001000 );

	//			Guest FS limit										00004808H
				__asm	MOV seg_selector, FS
				temp32 = 0;
				temp32 = GetSegmentDescriptorLimit( gdt_base, seg_selector );
//				//Log( "Setting Guest FS limit" , 0x00001000 );
				WriteVMCS( 0x00004808, temp32 );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	//			Guest GS limit										0000480AH
				__asm	MOV seg_selector, GS
				temp32 = 0;
				temp32 = GetSegmentDescriptorLimit( gdt_base, seg_selector );
//				//Log( "Setting Guest GS limit" , 0xFFFFFFFF );
				WriteVMCS( 0x0000480A, 0xFFFFFFFF );

	//			Guest TR limit										0000480EH
				__asm
				{
					PUSH	EAX

					STR		AX
					MOV		mLDT, AX

					POP		EAX
				}
				temp32 = 0;
				temp32 = GetSegmentDescriptorLimit( gdt_base, mLDT );
//				//Log( "Setting Guest TR limit" , temp32 );
				WriteVMCS( 0x0000480E, temp32 );

	//			Guest GDTR limit									00004810H
//				//Log( "Setting Guest GDTR limit" , gdt_reg.Limit );
				WriteVMCS( 0x00004810, gdt_reg.Limit );

	//			Guest IDTR limit									00004812H
//				//Log( "Setting Guest IDTR limit" , idt_reg.Limit );
				WriteVMCS( 0x00004812, idt_reg.Limit );

				__asm	MOV		seg_selector, CS
				temp32 = seg_selector;
				temp32 >>= 3;
				temp32 *= 8;
				temp32 += (gdt_base + 5);			// CS Segment Descriptor
				__asm
				{
					PUSHAD
					MOV		EAX, temp32
					MOV		EBX, [EAX]
					MOV		temp32, EBX
					POPAD
				}
				temp32 &= 0x0000F0FF;
//				//Log( "Setting Guest CS access rights" , temp32 );
				WriteVMCS( 0x00004816, temp32 );

				__asm	MOV		seg_selector, DS
				temp32 = seg_selector;
				temp32 >>= 3;
				temp32 *= 8;
				temp32 += (gdt_base + 5);			// DS Segment Descriptor
				__asm
				{
					PUSHAD
					MOV		EAX, temp32
					MOV		EBX, [EAX]
					MOV		temp32, EBX
					POPAD
				}
				temp32 &= 0x0000F0FF;
//				//Log( "Setting Guest DS access rights" , temp32 );
				WriteVMCS( 0x0000481A, temp32 );

				__asm	MOV		seg_selector, ES
				temp32 = seg_selector;
				temp32 >>= 3;
				temp32 *= 8;
				temp32 += (gdt_base + 5);			// ES Segment Descriptor
				__asm
				{
					PUSHAD
					MOV		EAX, temp32
					MOV		EBX, [EAX]
					MOV		temp32, EBX
					POPAD
				}
				temp32 &= 0x0000F0FF;
//				//Log( "Setting Guest ES access rights" , temp32 );
				WriteVMCS( 0x00004814, temp32 );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// The following is the old code. This was also hardcoded. Not sure why. We simply put this back to
// normal and it started working just fine in Windows 7.

//				__asm	MOV		seg_selector, FS
//				temp32 = seg_selector;
//				temp32 >>= 3;
//				temp32 *= 8;
//				temp32 += (gdt_base + 5);			// FS Segment Descriptor
//				__asm
//				{
//					PUSHAD
//					MOV		EAX, temp32
//					MOV		EBX, [EAX]
//					MOV		temp32, EBX
//					POPAD
//				}
//				temp32 &= 0x0000F0FF;
//				temp32 &= 0xFFFF7FFF;				// Granularity Bit = 0
//				//Log( "Setting Guest FS access rights" , temp32 );
//				WriteVMCS( 0x0000481C, temp32 );


				__asm	MOV		seg_selector, FS
				temp32 = seg_selector;
				temp32 >>= 3;
				temp32 *= 8;
				temp32 += (gdt_base + 5);			// FS Segment Descriptor
				__asm
				{
					PUSHAD
					MOV		EAX, temp32
					MOV		EBX, [EAX]
					MOV		temp32, EBX
					POPAD
				}
				temp32 &= 0x0000F0FF;
//				//Log( "Setting Guest FS access rights" , temp32 );
				WriteVMCS( 0x0000481C, temp32 );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

				__asm	MOV		seg_selector, GS
				temp32 = seg_selector;
				temp32 >>= 3;
				temp32 *= 8;
				temp32 += (gdt_base + 5);			// GS Segment Descriptor
				__asm
				{
					PUSHAD
					MOV		EAX, temp32
					MOV		EBX, [EAX]
					MOV		temp32, EBX
					POPAD
				}
				temp32 &= 0x0000F0FF;
				SetBit( &temp32, 16 );				// Unusable
//				//Log( "Setting Guest GS access rights" , temp32 );
				WriteVMCS( 0x0000481E, temp32 );

				__asm	MOV		seg_selector, SS
				temp32 = seg_selector;
				temp32 >>= 3;
				temp32 *= 8;
				temp32 += (gdt_base + 5);			// SS Segment Descriptor
				__asm
				{
					PUSHAD
					MOV		EAX, temp32
					MOV		EBX, [EAX]
					MOV		temp32, EBX
					POPAD
				}
				temp32 &= 0x0000F0FF;
//				//Log( "Setting Guest SS access rights" , temp32 );
				WriteVMCS( 0x00004818, temp32 );

				__asm	STR		seg_selector
				temp32 = seg_selector;
				temp32 >>= 3;
				temp32 *= 8;
				temp32 += (gdt_base + 5);			// TR Segment Descriptor
				__asm
				{
					PUSHAD
					MOV		EAX, temp32
					MOV		EBX, [EAX]
					MOV		temp32, EBX
					POPAD
				}
				temp32 &= 0x0000F0FF;
//				//Log( "Setting Guest TR access rights" , temp32 );
				WriteVMCS( 0x00004822, temp32 );

	//			Guest LDTR access rights							00004820H
				temp32 = 0;
				SetBit( &temp32, 16 );			// Unusable
//				//Log( "Setting Guest LDTR access rights" , temp32 );
				WriteVMCS( 0x00004820, temp32 );

	//			Guest IA32_SYSENTER_CS								0000482AH
	//			(174H)
				ReadMSR( 0x174 );
//				//Log( "Setting Guest IA32_SYSENTER_CS" , (ULONG)msr.Lo );
				WriteVMCS( 0x0000482A, msr.Lo );

	//  **************************************
	//  *                                    *
	//  *	H.3.4 32-Bit Host-State Fields   *
	//  *                                    *
	//  **************************************
	//
	//			Host IA32_SYSENTER_CS								00004C00H
	//			(174H)
				ReadMSR( 0x174 );
//				//Log( "Setting Host IA32_SYSENTER_CS" , (ULONG)msr.Lo );
				WriteVMCS( 0x00004C00, msr.Lo );

	//  **********************************************
	//  *                                            *
	//  *	H.4.3 Natural-Width Guest-State Fields   *
	//  *                                            *
	//  **********************************************
	//
	//			Guest CR0											00006800H
				__asm
				{
					PUSH	EAX
					MOV		EAX, CR0
					MOV		temp32, EAX
					POP		EAX
				}

				ReadMSR( 0x486 );							// IA32_VMX_CR0_FIXED0
//				//Log( "IA32_VMX_CR0_FIXED0" , msr.Lo );

				ReadMSR( 0x487 );							// IA32_VMX_CR0_FIXED1
//				//Log( "IA32_VMX_CR0_FIXED1" , msr.Lo );

				SetBit( &temp32, 0 );		// PE
				SetBit( &temp32, 5 );		// NE
				SetBit( &temp32, 31 );		// PG
//				//Log( "Setting Guest CR0" , temp32 );
				WriteVMCS( 0x00006800, temp32 );

	//			Guest CR3											00006802H
				__asm
				{
					PUSH	EAX

					_emit	0x0F	// MOV EAX, CR3
					_emit	0x20
					_emit	0xD8

					MOV		temp32, EAX

					POP		EAX
				}
//				//Log( "Setting Guest CR3" , temp32 );
				WriteVMCS( 0x00006802, temp32 );

	//			Guest CR4											00006804H
				__asm
				{
					PUSH	EAX

					_emit	0x0F	// MOV EAX, CR4
					_emit	0x20
					_emit	0xE0

					MOV		temp32, EAX

					POP		EAX
				}

				ReadMSR( 0x488 );							// IA32_VMX_CR4_FIXED0
//				//Log( "IA32_VMX_CR4_FIXED0" , msr.Lo );

				ReadMSR( 0x489 );							// IA32_VMX_CR4_FIXED1
//				//Log( "IA32_VMX_CR4_FIXED1" , msr.Lo );

				SetBit( &temp32, 13 );		// VMXE
//				//Log( "Setting Guest CR4" , temp32 );
				WriteVMCS( 0x00006804, temp32 );

	//			Guest ES base										00006806H
				__asm	MOV		seg_selector, ES
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , seg_selector );
//				//Log( "Setting Guest ES Base" , temp32 );
				WriteVMCS( 0x00006806, temp32 );

	//			Guest CS base										00006808H
				__asm	MOV		seg_selector, CS
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , seg_selector );
//				//Log( "Setting Guest CS Base" , temp32 );
				WriteVMCS( 0x00006808, temp32 );

	//			Guest SS base										0000680AH
				__asm	MOV		seg_selector, SS
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , seg_selector );
//				//Log( "Setting Guest SS Base" , temp32 );
				WriteVMCS( 0x0000680A, temp32 );

	//			Guest DS base										0000680CH
				__asm	MOV		seg_selector, DS
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , seg_selector );
//				//Log( "Setting Guest DS Base" , temp32 );
				WriteVMCS( 0x0000680C, temp32 );

	//			Guest FS base										0000680EH
				__asm	MOV		seg_selector, FS
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , seg_selector );
//				//Log( "Setting Guest FS Base" , temp32 );
				WriteVMCS( 0x0000680E, temp32 );

	//			Guest TR base										00006814H
				__asm
				{
					PUSH	EAX

					STR		AX
					MOV		mLDT, AX

					POP		EAX
				}
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , mLDT );
//				//Log( "Setting Guest TR Base" , temp32 );
				WriteVMCS( 0x00006814, temp32 );

	//			Guest GDTR base										00006816H
				__asm
				{
					SGDT	gdt_reg
				}
				temp32 = 0;
				temp32 = gdt_reg.BaseHi;
				temp32 <<= 16;
				temp32 |= gdt_reg.BaseLo;
//				//Log( "Setting Guest GDTR Base" , temp32 );
				WriteVMCS( 0x00006816, temp32 );

	//			Guest IDTR base										00006818H
				__asm
				{
					SIDT	idt_reg
				}
				temp32 = 0;
				temp32 = idt_reg.BaseHi;
				temp32 <<= 16;
				temp32 |= idt_reg.BaseLo;
//				//Log( "Setting Guest IDTR Base" , temp32 );
				WriteVMCS( 0x00006818, temp32 );

	//			Guest RFLAGS										00006820H
				__asm
				{
					PUSHAD

					PUSHFD

					MOV		EAX, 0x00006820

					// VMWRITE	EAX, [ESP]
					_emit	0x0F
					_emit	0x79
					_emit	0x04
					_emit	0x24

					POP		eFlags

					POPAD
				}
//				//Log( "Guest EFLAGS" , eFlags );

	//			Guest IA32_SYSENTER_ESP								00006824H
	//			MSR (175H)
				ReadMSR( 0x175 );
//				//Log( "Setting Guest IA32_SYSENTER_ESP" , msr.Lo );
				WriteVMCS( 0x00006824, msr.Lo );

	//			Guest IA32_SYSENTER_EIP								00006826H
	//			MSR (176H)
				ReadMSR( 0x176 );
//				//Log( "Setting Guest IA32_SYSENTER_EIP" , msr.Lo );
				WriteVMCS( 0x00006826, msr.Lo );


	//  *********************************************
	//  *                                           *
	//  *	H.4.4 Natural-Width Host-State Fields   *
	//  *                                           *
	//  *********************************************
	//
	//			Host CR0											00006C00H
				__asm
				{
					PUSH	EAX
					MOV		EAX, CR0
					MOV		temp32, EAX
					POP		EAX
				}
				SetBit( &temp32, 5 );								// Set NE Bit
//				//Log( "Setting Host CR0" , temp32 );
				WriteVMCS( 0x00006C00, temp32 );

	//			Host CR3											00006C02H
				__asm
				{
					PUSH	EAX

					_emit	0x0F	// MOV EAX, CR3
					_emit	0x20
					_emit	0xD8

					MOV		temp32, EAX

					POP		EAX
				}
//				//Log( "Setting Host CR3" , temp32 );
				WriteVMCS( 0x00006C02, temp32 );

	//			Host CR4											00006C04H
				__asm
				{
					PUSH	EAX

					_emit	0x0F	// MOV EAX, CR4
					_emit	0x20
					_emit	0xE0

					MOV		temp32, EAX

					POP		EAX
				}
//				//Log( "Setting Host CR4" , temp32 );
				WriteVMCS( 0x00006C04, temp32 );

	//			Host FS base										00006C06H
				__asm	MOV		seg_selector, FS
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , seg_selector );
//				//Log( "Setting Host FS Base" , temp32 );
				WriteVMCS( 0x00006C06, temp32 );

	//			Host TR base										00006C0AH
				__asm
				{
					PUSH	EAX

					STR		AX
					MOV		mLDT, AX

					POP		EAX
				}
				temp32 = 0;
				temp32 = GetSegmentDescriptorBase( gdt_base , mLDT );
//				//Log( "Setting Host TR Base" , temp32 );
				WriteVMCS( 0x00006C0A, temp32 );

	//			Host GDTR base										00006C0CH
				__asm
				{
					SGDT	gdt_reg
				}
				temp32 = 0;
				temp32 = gdt_reg.BaseHi;
				temp32 <<= 16;
				temp32 |= gdt_reg.BaseLo;
//				//Log( "Setting Host GDTR Base" , temp32 );
				WriteVMCS( 0x00006C0C, temp32 );

	//			Host IDTR base										00006C0EH
				__asm
				{
					SIDT	idt_reg
				}
				temp32 = 0;
				temp32 = idt_reg.BaseHi;
				temp32 <<= 16;
				temp32 |= idt_reg.BaseLo;
//				//Log( "Setting Host IDTR Base" , temp32 );
				WriteVMCS( 0x00006C0E, temp32 );

	//			Host IA32_SYSENTER_ESP								00006C10H
	//			MSR (175H)
				ReadMSR( 0x175 );
//				//Log( "Setting Host IA32_SYSENTER_ESP" , msr.Lo );
				WriteVMCS( 0x00006C10, msr.Lo );

	//			Host IA32_SYSENTER_EIP								00006C12H
	//			MSR (176H)
				ReadMSR( 0x176 );
//				//Log( "Setting Host IA32_SYSENTER_EIP" , msr.Lo );
				WriteVMCS( 0x00006C12, msr.Lo );

	//	(5)	Issue a sequence of VMWRITEs to initialize various host-state area
	//		fields in the working VMCS. The initialization sets up the context
	//		and entry-points to the VMM VIRTUAL-MACHINE MONITOR PROGRAMMING
	//		CONSIDERATIONS upon subsequent VM exits from the guest. Host-state
	//		fields include control registers (CR0, CR3 and CR4), selector fields
	//		for the segment registers (CS, SS, DS, ES, FS, GS and TR), and base-
	//		address fields (for FS, GS, TR, GDTR and IDTR; RSP, RIP and the MSRs
	//		that control fast system calls).
	//

	//	(6)	Use VMWRITEs to set up the various VM-exit control fields, VM-entry
	//		control fields, and VM-execution control fields in the VMCS. Care
	//		should be taken to make sure the settings of individual fields match
	//		the allowed 0 and 1 settings for the respective controls as reported
	//		by the VMX capability MSRs (see Appendix G). Any settings inconsistent
	//		with the settings reported by the capability MSRs will cause VM
	//		entries to fail.

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	// In this part of the code, we setup all of the different architectural events we wish to
	// trap on. That is, this part of the code will tell the hypervisor which arcitectural
	// events we wish to have control of.

// SC ---------------------------------------------------------------------------------------------------------------------------------------
// MoRE
    // Turn on EPT if the identity map was created properly
    if (EptPml4TablePointer != NULL)
    {
        EnableEpt((uint32) EptPml4TablePointer);
    }
    else
    {
        goto Abort;
    }
// End MoRE 

	//	(7)	Use VMWRITE to initialize various guest-state area fields in the
	//		working VMCS. This sets up the context and entry-point for guest
	//		execution upon VM entry. Chapter 22 describes the guest-state loading
	//		and checking done by the processor for VM entries to protected and
	//		virtual-8086 guest execution.
	//

	// Clear the VMX Abort Error Code prior to VMLAUNCH
	//
	RtlZeroMemory( (pVMCSRegion + 4), 4 );
//	//Log( "Clearing VMX Abort Error Code" , *(pVMCSRegion + 4) );

	//	Set EIP, ESP for the Guest right before calling VMLAUNCH
	//
//	//Log( "Setting Guest ESP" , GuestStack );
	WriteVMCS( 0x0000681C, (ULONG)GuestStack );

//	//Log( "Setting Guest EIP" , GuestReturn );
	WriteVMCS( 0x0000681E, (ULONG)GuestReturn );

	/*
	//	Allocate some stack space for the VMEntry and VMMHandler.
	//
	HighestAcceptableAddress.QuadPart = 0xFFFFFFFF;
	FakeStack = MmAllocateContiguousMemory( 0x2000, HighestAcceptableAddress );
	//Log( "FakeStack" , FakeStack );
	*/

	//	Set EIP, ESP for the Host right before calling VMLAUNCH
	//
//	//Log( "Setting Host ESP" , ((ULONG)FakeStack + 0x1FFF) );
	WriteVMCS( 0x00006C14, ((ULONG)FakeStack + 0x1FFF) );

//	//Log( "Setting Host EIP" , hypervisor_entry_point );
	WriteVMCS( 0x00006C16, (ULONG)hypervisor_entry_point );

	////////////////
	//            //
	//  VMLAUNCH  //
	//            //
	////////////////
	__asm
	{
		_emit	0x0F	// VMLAUNCH
		_emit	0x01
		_emit	0xC2
	}

	__asm
	{
		PUSHFD
		POP		eFlags
	}

 	Log( "VMLAUNCH Failure" , 0xDEADF00D )

	if( eFlags.CF != 0 || eFlags.ZF != 0 || TRUE )
	{
		//
		//	Get the ERROR number using VMCS field 00004400H
		//
		__asm
		{
			PUSHAD

			MOV		EAX, 0x00004400

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		ErrorCode, EBX

			POPAD
		}

		Log( "VM Instruction Error" , ErrorCode );
	}

Abort:

	ScrubTheLaunch = 1;
	__asm
	{
		MOV		ESP, GuestStack
		JMP		GuestReturn
	}
}

////////////////////
//                //
//  DriverUnload  //
//                //
////////////////////
DRIVER_UNLOAD DriverUnload;
VOID DriverUnload( IN PDRIVER_OBJECT DriverObject )
{

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// We do not support multi-core CPUs

//	DbgPrint( "[vmm-unload] Active Processor Bitmap  [%08X]\n", (ULONG)KeQueryActiveProcessors( ) );
//
//	DbgPrint( "[vmm-unload] Disabling VMX mode on CPU 0.\n" );
//	KeSetSystemAffinityThread( (KAFFINITY) 0x00000001 );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	__asm
	{
		PUSHAD
		MOV		EAX, 0x12345678

		_emit 0x0F		// VMCALL
		_emit 0x01
		_emit 0xC1

		POPAD
	}

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// We no longer do a VMX exit so these structures should not be removed from memory.
// FIXME: This causes a memory leak. This loader simply needs to be re-written to fix this.

//	DbgPrint( "[vmm-unload] Freeing memory regions.\n" );
//
//	MmFreeNonCachedMemory( pVMXONRegion , 4096 );
//	MmFreeNonCachedMemory( pVMCSRegion , 4096 );
//	ExFreePoolWithTag( FakeStack, 'kSkF' );

// MoRE
#ifdef MONITOR_PROCS
    // Remove callback
    PsSetCreateProcessNotifyRoutine(&processCreationMonitor, TRUE);
#endif
    // Disable EPT and free memory
    DisableEpt();
    FreeEptIdentityMap(EptPml4TablePointer);
    pagingEndMappingOperations(&memContext);
// End MoRE

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	DbgPrint( "[vmm-unload] Driver Unloaded.\n");

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------
	// Un-initialize the root commands.

	unload_hypervisor();

	// --------------------------------------------------------------------------------

	Beep( 0 );

    if (!ScrubTheLaunch)
    {
    	Log( "****************************************", 0 );
    	Log( "*                                      *", 0 );
    	Log( "* VMM Unloaded                         *", 0 );
    	Log( "*                                      *", 0 );
    	Log( "****************************************", 0 );
	}
// SC ---------------------------------------------------------------------------------------------------------------------------------------

}

////////////////////
//                //
//  Driver Entry  //
//                //
////////////////////
DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath )
{
	NTSTATUS	ntStatus = STATUS_UNSUCCESSFUL;

	ULONG		EntryEFlags = 0;
	ULONG		cr4 = 0;

	ULONG		EntryEAX = 0;
	ULONG		EntryECX = 0;
	ULONG		EntryEDX = 0;
	ULONG		EntryEBX = 0;
	ULONG		EntryESP = 0;
	ULONG		EntryEBP = 0;
	ULONG		EntryESI = 0;
	ULONG		EntryEDI = 0;
    PHYSICAL_ADDRESS phys = {0};

	DriverObject->DriverUnload = DriverUnload;

//	//Log( "Driver Routines" , 0 );
//	//Log( "---------------" , 0 );
//	//Log( "   Driver Entry", DriverEntry );
//	//Log( "   Driver Unload", DriverUnload );
//	//Log( "   StartVMX", StartVMX );
//	//Log( "   hypervisor_entry_point", hypervisor_entry_point );

	//	Check if PAE is enabled.
	//
	__asm
	{
		PUSH	EAX

		_emit	0x0F	// MOV	EAX, CR4
		_emit	0x20
		_emit	0xE0

		MOV		cr4, EAX

		POP		EAX
	}
	if( cr4 & 0x00000020 )
	{
		Log( "******************************" , 0 );
		Log( "Error : PAE must be disabled." , 0 );
		//Log( "Add the following to boot.ini:" , 0 );
		//Log( "  /noexecute=alwaysoff /nopae" , 0 );
		Log( "******************************" , 0 );
		return STATUS_UNSUCCESSFUL;
	}

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	load_hypervisor();

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	//	Allocate the VMXON region memory.
	//
	pVMXONRegion = MmAllocateNonCachedMemory( 4096 );
	if( pVMXONRegion == NULL )
	{
		//Log( "ERROR : Allocating VMXON Region memory." , 0 );
		return STATUS_UNSUCCESSFUL;
	}
//	//Log( "VMXONRegion virtual address" , pVMXONRegion );
	RtlZeroMemory( pVMXONRegion, 4096 );
	PhysicalVMXONRegionPtr = MmGetPhysicalAddress( pVMXONRegion );
//	//Log( "VMXONRegion physical address" , PhysicalVMXONRegionPtr.LowPart );

	//	Allocate the VMCS region memory.
	//
	pVMCSRegion = MmAllocateNonCachedMemory( 4096 );
	if( pVMCSRegion == NULL )
	{
		//Log( "ERROR : Allocating VMCS Region memory." , 0 );
		MmFreeNonCachedMemory( pVMXONRegion , 4096 );
		return STATUS_UNSUCCESSFUL;
	}
//	//Log( "VMCSRegion virtual address" , pVMCSRegion );
	RtlZeroMemory( pVMCSRegion, 4096 );
	PhysicalVMCSRegionPtr = MmGetPhysicalAddress( pVMCSRegion );
//	//Log( "VMCSRegion physical address" , PhysicalVMCSRegionPtr.LowPart );

	//	Allocate stack for the VM Exit Handler.
	//
	FakeStack = ExAllocatePoolWithTag( NonPagedPool , 0x2000, 'kSkF' );
	if( FakeStack == NULL )
	{
		//Log( "ERROR : Allocating VM Exit Handler stack memory." , 0 );
		MmFreeNonCachedMemory( pVMXONRegion , 4096 );
		MmFreeNonCachedMemory( pVMCSRegion , 4096 );
		return STATUS_UNSUCCESSFUL;
	}
//	//Log( "FakeStack" , FakeStack );

// MoRE
    // Allocate and initialize the EPT indentity map
    EptPml4TablePointer = InitEptIdentityMap();
    pagingInitMappingOperations(&memContext, NUM_PAGES_ALLOC);
// End MoRE
	__asm
	{
		CLI
		MOV		GuestStack, ESP
	}

	//	Save the state of the architecture.
	//
	__asm
	{
		PUSHAD
		POP		EntryEDI
		POP		EntryESI
		POP		EntryEBP
		POP		EntryESP
		POP		EntryEBX
		POP		EntryEDX
		POP		EntryECX
		POP		EntryEAX
		PUSHFD
		POP		EntryEFlags
	}

	StartVMX( );

	//	Restore the state of the architecture.
	//
	__asm
	{
		PUSH	EntryEFlags
		POPFD
		PUSH	EntryEAX
		PUSH	EntryECX
		PUSH	EntryEDX
		PUSH	EntryEBX
		PUSH	EntryESP
		PUSH	EntryEBP
		PUSH	EntryESI
		PUSH	EntryEDI
		POPAD
	}

	__asm
	{
		STI
		MOV		ESP, GuestStack
	}

//	//Log( "Running on Processor" , KeGetCurrentProcessorNumber() );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// This is dumb because it could cause a crash, and it gets you nothing since you
// will have to restart with a new attempt at launching anyways.

	if( ScrubTheLaunch == 1 )
	{
		Log( "ERROR : Launch aborted." , 0 );
		MmFreeNonCachedMemory( pVMXONRegion , 4096 );
		MmFreeNonCachedMemory( pVMCSRegion , 4096 );
		ExFreePoolWithTag( FakeStack, 'kSkF' );
		return STATUS_SUCCESS;
	}
    
// MoRE
    // Setup the code needed to monitor process load
#ifdef MONITOR_PROCS   
    // Setup callback for new process creation monitoring
    PsSetCreateProcessNotifyRoutine(&processCreationMonitor, FALSE);
#endif
    // Uncomment the below function to run a drop 1-like demo
    //splitPage();
// MoRE

// SC ---------------------------------------------------------------------------------------------------------------------------------------

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	Log( "****************************************", 0 );
	Log( "*                                      *", 0 );
	Log( "*    Executive Hypervisor Loaded       *", 0 );
	Log( "*                                      *", 0 );
	Log( "****************************************", 0 );
	DbgPrint( "\r\n" );

// SC ---------------------------------------------------------------------------------------------------------------------------------------

	return STATUS_SUCCESS;
}
