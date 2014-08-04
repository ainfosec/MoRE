#include "Ntifs.h"
#include "Wdm.h"
#include "ntddk.h"
#include "..\pe.h"
#include "..\paging.h"
#include "procmon.h"
#include "hypervisor.h"
#include "hypervisor_loader.h"
#include "hypervisor_msr.h"
#include "ept.h"
#include "log.h"

// ================================================================================
// Define global variables.

// Store the Guest State.
struct GUEST_STATE GuestSTATE;

// Define the Exit Handlers Arrays. Note: Not all of the exit handlers are implemented
// At some point, every supported exit, should have an exit handler array, that acts like
// a jump table. Each module can register with an exit handler to that it can ack on the
// exit handler.
exit_reason_dispatch_handler	vmcall_handlers[RC_EAX__NUM]	= {0};
exit_reason_dispatch_handler	cra_handlers[RC_EAX__NUM]	= {0};
exit_reason_dispatch_handler	msr_handlers[RC_EAX__NUM]	= {0};

// ================================================================================
// Load / Unload functions.

void load_hypervisor( void )
{
	// --------------------------------------------------------------------------------
	// Register the exit dispatch handlers.

	// Control Register Access.
	vmcall_handlers[0]		= 0x0;
	vmcall_handlers[rc_eax__exec]	= exit_reason_dispatch_handler__exec_vmcall;
	vmcall_handlers[rc_eax__msr]	= exit_reason_dispatch_handler__msr_vmcall;
	vmcall_handlers[rc_eax__log]	= exit_reason_dispatch_handler__log_vmcall;

	// Control Register Access.
	cra_handlers[0]			= 0x0;
	cra_handlers[rc_eax__exec]	= exit_reason_dispatch_handler__exec_cra;
	cra_handlers[rc_eax__msr]	= 0x0;
	cra_handlers[rc_eax__log]	= exit_reason_dispatch_handler__log_cra;

	// Model Specific Register Access
	msr_handlers[0]			= 0x0;
	msr_handlers[rc_eax__exec]	= 0x0;
	msr_handlers[rc_eax__msr]	= exit_reason_dispatch_handler__msr_msr;
	msr_handlers[rc_eax__log]	= 0x0;

	// --------------------------------------------------------------------------------
	// Initialize the root commands.

	lc__init();	// Log Command
	mc__init();	// MSR Command
}

void unload_hypervisor( void )
{
	lc__exit();	// Log Command
	mc__exit();	// MSR Command
}

// ================================================================================
// Define the hypervior's internal functions.

// The following code, looks to see if any handlers are registered. If they are,
// the code is executed. The idea here, is that we do not know what we want to do
// with these exit reasons at the time of writing this. So, the functionaility is
// sent to dispatch handlers that take care of the grunt work.

void dispatch_exit_reason_handlers( exit_reason_dispatch_handler * handlers )
{
	// Defone the local variables.
	unsigned int i;

	// Loop through the root command handlers.
	for ( i = 1; i < RC_EAX__NUM; i++ )
	{
		// If the handler is not zero'd out,
		// jump to it. If the handler is zero'd out
		// ignore it as this means the room command has not
		// registered with this exit reason.
		if ( handlers[i] != 0x0 )
		{
			// Jump to the registered handler.
			handlers[i]( &GuestSTATE );
		}
	}

}

// ================================================================================
// Read / Write VMCS

unsigned int ReadVMCS( unsigned int encoding )
{
	// Define local variables.
	unsigned int result;

	__asm
	{
		PUSHAD

		MOV EAX, encoding

		// VMREAD  EBX, EAX
		_emit	0x0F
		_emit	0x78
		_emit	0xC3

		MOV result, EBX

		POPAD
	}

	// Return the result.
	return result;
}

void WriteVMCS( unsigned int encoding, unsigned int value )
{
	__asm
	{
		PUSHAD

		MOV EAX, encoding
		MOV EBX, value

		// VMWRITE EAX, EBX
		_emit	0x0F
		_emit	0x79
		_emit	0xC3

		POPAD
	}
}

// ================================================================================
// Exit Dispatch Handlers.

// VMCALL
void exit_reason_dispatch_handler__exec_vmcall( struct GUEST_STATE * GuestSTATE )
{
	unsigned int GuestEAX = GuestSTATE->GuestEAX;
	unsigned int GuestEBX = GuestSTATE->GuestEBX;
	unsigned int GuestECX = GuestSTATE->GuestECX;
	unsigned int GuestEDX = GuestSTATE->GuestEDX;
	unsigned int GuestESI = GuestSTATE->GuestESI;
	unsigned int GuestEDI = GuestSTATE->GuestEDI;

	if( GuestEAX == 0x12345678 )
	{
		// Leave VMX Non-root.
		disable_exec_hypervisor();
	}
    
// MoRE
    if (GuestEAX == VMCALL_INIT_SPLIT)
    {
        //Log("Init EIP", GuestSTATE->GuestEIP);
        if (GuestEBX == 0)
        {
            Beep(1);
            while (1) {};
        }
        init_split((TlbTranslation *) GuestEBX);
    }
    
    if (GuestEAX == VMCALL_END_SPLIT)
    {
        //Log("End EIP", GuestSTATE->GuestEIP);
        end_split(splitPages);
    }
    // This call might happen at DIRQL, it shouldn't but it's possible
    // As such, the kernel functions used must be kept to a minimum
    if (GuestEAX == VMCALL_MEASURE)
    {
        PHYSICAL_ADDRESS phys = {0};
        uint8 *pePtr;
        // If we can safely measure the PE, do so
        if (KeGetCurrentIrql() == 0)
        {
            phys.LowPart = GuestEBX;
#ifdef SPLIT_TLB
            DbgPrint("Checksum of proc (data copy): %x\r\n", 
                    peChecksumExecSections(targetPePtr, 
                                        (void *) GuestECX, 
                                        targetProc, 
                                        &apcstate, 
                                        targetPhys));
            DbgPrint("Checksum of proc (exec copy): %x\r\n", 
                    peChecksumBkupExecSections(targetPePtr, 
                                        (void *) GuestECX, 
                                        targetProc, 
                                        &apcstate,
                                        targetPhys));
            //DbgPrint("Exec: %d Data: %d Thrash: %d\r\n", ExecExits, DataExits, Thrashes);
#endif
#ifndef SPLIT_TLB
            DbgPrint("Checksum of proc: %x\r\n", 
                    peChecksumExecSections(targetPePtr, 
                                        (void *) GuestECX, 
                                        targetProc, 
                                        &apcstate, 
                                        targetPhys));
#endif
        }
    }
// End MoRE

/**
// MoRE
	// Forward this vmcall to the smm hypervisor.
	__asm
	{
		PUSHAD

		MOV EAX, GuestEAX
		MOV EBX, GuestEBX
		MOV ECX, GuestECX
		MOV EDX, GuestEDX
		MOV EDI, GuestEDI
		MOV ESI, GuestESI

		// VMCALL
		_emit 0x0F
		_emit 0x01
		_emit 0xC1

		POPAD
	}
// End MoRE
*/
}

// Control Register Access
void exit_reason_dispatch_handler__exec_cra( struct GUEST_STATE * GuestSTATE )
{
	// Define local variables.
	unsigned int ExitQualification	= ReadVMCS( EXIT_QUALIFICATION );
	unsigned int GuestCR3		= ReadVMCS( GUEST_CR3 );

	// Parse the exit qualification field.
	unsigned int movcrControlRegister	 = ( ( ExitQualification & 0x0000000F ) );
	unsigned int movcrAccessType		 = ( ( ExitQualification & 0x00000030 ) >> 4 );
	unsigned int movcrOperandType		 = ( ( ExitQualification & 0x00000040 ) >> 6 );
	unsigned int movcrGeneralPurposeRegister = ( ( ExitQualification & 0x00000F00 ) >> 8 );

	// ----------------------------------------------------------------------
	// Control Register Access (CR3 <-- reg32)

	if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 0 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestEAX );
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 1 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestECX );
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 2 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestEDX );
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 3 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestEBX );
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 4 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestESP );
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 5 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestEBP );
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 6 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestESI );
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 7 )
	{
		WriteVMCS( GUEST_CR3, GuestSTATE->GuestEDI );
	}
    
// MoRE 
#ifdef SPLIT_TLB   
    // NOTE: All the calls made from this block must be able to support operation at DIRQL
    // This VMEXIT only occurs in the kernel, so we must be careful about what is done here!  
    if (ReadVMCS(GUEST_CR3) == targetCR3 && splitPages != NULL)
    {
        EptPteEntry *eptpte = NULL;
        uint32 i;
        for (i = 0; i < appsize / PAGE_SIZE; i++)
        {
            if(targetPtes[i] != NULL)
            {   
                TlbTranslation *ptr = getTlbTranslation(splitPages, targetPtes[i]->address << 12);
                if (ptr == NULL)
                {
                    AppendTlbTranslation(splitPages, targetPtes[i]->address << 12, 
                                        (uint8 *) targetPeVirt + (i * PAGE_SIZE));
                }
            }
        }
    }
#endif
// End MoRE

	// ----------------------------------------------------------------------
	// Control Register Access (reg32 <-- CR3)

	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 0 )
	{
		GuestSTATE->GuestEAX = GuestCR3;
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 1 )
	{
		GuestSTATE->GuestECX = GuestCR3;
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 2 )
	{
		GuestSTATE->GuestEDX = GuestCR3;
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 3 )
	{
		GuestSTATE->GuestEBX = GuestCR3;
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 4 )
	{
		GuestSTATE->GuestESP = GuestCR3;
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 5 )
	{
		GuestSTATE->GuestEBP = GuestCR3;
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 6 )
	{
		GuestSTATE->GuestESI = GuestCR3;
	}
	else if( movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 7 )
	{
		GuestSTATE->GuestEDI = GuestCR3;
	}
    
    // MoRE
    // This is needed as it is not done if VPID is enabled
    InvVpidAllContext();
    // End MoRE
}

// ================================================================================
// Define the hypervisor's entry point.

__declspec( naked ) void hypervisor_entry_point( void )
{
	__asm	CLI

	// Save CPU State
	__asm	MOV GuestSTATE.GuestEAX, EAX
	__asm	MOV GuestSTATE.GuestEBX, EBX
	__asm	MOV GuestSTATE.GuestECX, ECX
	__asm	MOV GuestSTATE.GuestEDX, EDX
	__asm	MOV GuestSTATE.GuestEDI, EDI
	__asm	MOV GuestSTATE.GuestESI, ESI
	__asm	MOV GuestSTATE.GuestEBP, EBP

	GuestSTATE.GuestESP = ReadVMCS( GUEST_RSP );
	GuestSTATE.GuestEIP = ReadVMCS( GUEST_RIP );

	// ----------------------------------------------------------------------

	hypervisor_exit_handler();

	// ----------------------------------------------------------------------

	WriteVMCS( GUEST_RSP , GuestSTATE.GuestESP );
	WriteVMCS( GUEST_RIP , GuestSTATE.GuestEIP );

	__asm	MOV EAX, GuestSTATE.GuestEAX
	__asm	MOV EBX, GuestSTATE.GuestEBX
	__asm	MOV ECX, GuestSTATE.GuestECX
	__asm	MOV EDX, GuestSTATE.GuestEDX
	__asm	MOV EDI, GuestSTATE.GuestEDI
	__asm	MOV ESI, GuestSTATE.GuestESI
	__asm	MOV EBP, GuestSTATE.GuestEBP

	__asm	STI

	// VMRESUME
	__asm
	{
		_emit	0x0F
		_emit	0x01
		_emit	0xC3
	}
}

void advance_eip( void )
{
	// Update the local cache of the Guest State.
	GuestSTATE.GuestEIP += ReadVMCS( VM_EXIT_INSTRUCTION_LEN );
}

void hypervisor_exit_handler( void )
{
    uint32 i, j = 0;
	// Dispatch exit handlers.
	switch( ReadVMCS( VM_EXIT_REASON ) )
	{
		case EXIT_REASON_VMLAUNCH:
		case EXIT_REASON_VMCLEAR:
        case EXIT_REASON_VMPTRLD:
		case EXIT_REASON_VMPTRST:
		case EXIT_REASON_VMREAD:
		case EXIT_REASON_VMRESUME:
		case EXIT_REASON_VMWRITE:
		case EXIT_REASON_VMXOFF:
		case EXIT_REASON_VMXON:

			// Advance the instruction pointer.
			advance_eip();

			// Done.
			break;


		case EXIT_REASON_VMCALL:

			// Advance the instruction pointer.
			advance_eip();

			// Dispatch the exit reason.
			dispatch_exit_reason_handlers( vmcall_handlers );

			// Done.
			break;


		case EXIT_REASON_INVD:

			// Advance the instruction pointer.
			advance_eip();

			__asm
			{
				//INVD
				_emit 0x0F
				_emit 0x08
			}

			// Done.
			break;


		case EXIT_REASON_MSR_READ:
		case EXIT_REASON_MSR_WRITE:

			// Advance the instruction pointer.
			advance_eip();

			// Dispatch the exit reason.
			dispatch_exit_reason_handlers( msr_handlers );

			// Done.
			break;


		case EXIT_REASON_CPUID:

			// Advance the instruction pointer.
			advance_eip();

			__asm
			{
				PUSHAD

				MOV EAX, GuestSTATE.GuestEAX

				CPUID

				MOV GuestSTATE.GuestEAX, EAX
				MOV GuestSTATE.GuestEBX, EBX
				MOV GuestSTATE.GuestECX, ECX
				MOV GuestSTATE.GuestEDX, EDX

				POPAD
			}

			// Done.
			break;


		case EXIT_REASON_CR_ACCESS:

			// Advance the instruction pointer.
			advance_eip();

			// Dispatch the exit reason.
			dispatch_exit_reason_handlers( cra_handlers );

			// Done.
			break;

// MoRE
        // This VMEXIT means that the EPT tables are incorrectly configured
        case EXIT_REASON_EPT_MISCONFIG:
            //Beep(1);
            hypervisor_panic();
            break;
            
        // This is a trap
        case EXIT_REASON_EXCEPTION_NMI:
            // Handle the trap exception
            exit_reason_dispatch_handler__exec_trap(&GuestSTATE);
            break;
            
        // An EPT 'page-fault' if you will
        case EXIT_REASON_EPT_VIOLATION:
            // Handle the EPT violation
            exit_reason_dispatch_handler__exec_ept(&GuestSTATE);
            break;
            
        case EXIT_REASON_TRIPLE_FAULT:
            while (1)
            {
                Beep(1);
                for (i = 0; i < 100000; i++)
                {
                    __asm 
                    {
                        NOP
                    }
                }
                Beep(0);
                for (i = 0; i < 100000; i++)
                {
                    __asm 
                    {
                        NOP
                    }
                }
            }
            break;
// End MoRE            
    
		default:

			// Unhandled Exit Reason.
            while (1)
            {
                Beep(1);
                for (i = 0; i < 100000; i++)
                {
                    __asm 
                    {
                        NOP
                    }
                }
                Beep(0);
                for (i = 0; i < 100000; i++)
                {
                    __asm 
                    {
                        NOP
                    }
                }
            }

			hypervisor_panic();
			// Done.
			break;

	}
}

void hypervisor_panic( void )
{
	// FIXME: We should add some code here that does something useful
	// instead of just halting.
    
	// Tell the debugger that a panic occured.
	Log( "PANIC", 0xDEADBEEF );

	__asm	CLI
	__asm	HLT
}

__declspec( naked ) void disable_exec_hypervisor( void )
{
	// Restore the register state of the guest.
	__asm	MOV EAX, GuestSTATE.GuestEAX
	__asm	MOV EBX, GuestSTATE.GuestEBX
	__asm	MOV ECX, GuestSTATE.GuestECX
	__asm	MOV EDX, GuestSTATE.GuestEDX
	__asm	MOV ESI, GuestSTATE.GuestESI
	__asm	MOV EDI, GuestSTATE.GuestEDI
	__asm	MOV EBP, GuestSTATE.GuestEBP

	// Restore the stack
	__asm	MOV ESP, GuestSTATE.GuestESP

	__asm	STI

	// Jump back the Guest in Root Mode. This
	// basically starts executing the Guest in
	// VMX Root Mode meaning its no longer in
	// a virtual macine.
	__asm	JMP GuestSTATE.GuestEIP
}
