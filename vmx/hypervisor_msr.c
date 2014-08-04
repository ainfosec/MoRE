#include "hypervisor.h"
#include "hypervisor_msr.h"

#include "ntddk.h"

#define MAX_NUM_MSR_SLOTS 5

struct MSR_SLOT msr_write_slots[MAX_NUM_MSR_SLOTS] = {0};

// ================================================================================
// Define the init/exit routines for this root command module.

void mc__init( void )
{
	// Define local variables.
	unsigned int i = 0;

	// Loop through all of the MSR Slots and clear them out.
	for ( i = 0; i < MAX_NUM_MSR_SLOTS; i++ )
	{
		// Write
		msr_write_slots[i].set		= 0x0;
		msr_write_slots[i].msr_num	= 0x0;
	}
}

void mc__exit( void )
{

}

// ================================================================================
// Exit Dispatch Handlers.

// VMCall MSR Protocol
//
// EAX: MSR
// EBX: Block 0 = Read, 1 = Write
// ECX: Slot Number
// EDX: MSR Number
// ESI: Unused
// EDI: Unused

void exit_reason_dispatch_handler__msr_vmcall( struct GUEST_STATE * GuestSTATE )
{
	// Check the guest state to make sure the command sent belongs
	// to this module.
	if ( GuestSTATE->GuestEAX == rc_eax__msr )
	{
//		DbgPrint( "MSR Command: \n" );
//		DbgPrint( "    - GuestEAX: 0x%x\n", GuestSTATE->GuestEAX );
//		DbgPrint( "    - GuestEBX: 0x%x\n", GuestSTATE->GuestEBX );
//		DbgPrint( "    - GuestECX: 0x%x\n", GuestSTATE->GuestECX );
//		DbgPrint( "    - GuestEDX: 0x%x\n", GuestSTATE->GuestEDX );
//		DbgPrint( "    - GuestESI: 0x%x\n", GuestSTATE->GuestESI );
//		DbgPrint( "    - GuestEDI: 0x%x\n", GuestSTATE->GuestEDI );

		// Make sure the slot number is correct.
		if ( GuestSTATE->GuestECX >= MAX_NUM_MSR_SLOTS )
		{
			Log( "ERROR: MAX_NUM_MSR_SLOTS - %d\n", MAX_NUM_MSR_SLOTS );

			// Error.
			return;
		}

		// Block Reads
		if ( GuestSTATE->GuestEBX == mc_ebx__rd )
		{
			msr_write_slots[GuestSTATE->GuestECX].set	= 0x1;
			msr_write_slots[GuestSTATE->GuestECX].msr_num	= GuestSTATE->GuestEDX;
		}

		// Block Writes
		if ( GuestSTATE->GuestEBX == mc_ebx__wr )
		{
			msr_write_slots[GuestSTATE->GuestECX].set	= 0x1;
			msr_write_slots[GuestSTATE->GuestECX].msr_num	= GuestSTATE->GuestEDX;
		}
	}
}



void exit_reason_dispatch_handler__msr_msr( struct GUEST_STATE * GuestSTATE )
{
	// Store the state of the cpu. This is done because pointers
	// cannot be used in assembly.
	unsigned int GuestEAX = GuestSTATE->GuestEAX;
	unsigned int GuestEBX = GuestSTATE->GuestEBX;
	unsigned int GuestECX = GuestSTATE->GuestECX;
	unsigned int GuestEDX = GuestSTATE->GuestEDX;

	// Define an interator.
	unsigned int i = 0;

	// Loop through all of the msr slots and figure out
	// if this a slot matches the msr being written to.
	// If it does, apply the mask so that the msr
	// is protected correctly.
	for ( i = 0; i < MAX_NUM_MSR_SLOTS; i++ )
	{
		// Check to see if the msr number matches a slot.
		if ( msr_write_slots[i].msr_num == GuestECX )
		{
			Log( "*** Protected MSR Being Blocked", GuestECX );

			// Do not do anything. This basically skips
			// the instruction.
			return;
		}
	}

	// Switch between a read and a write.
	switch( ReadVMCS( VM_EXIT_REASON ) )
	{
		case EXIT_REASON_MSR_READ:

			__asm
			{
				PUSHAD

				MOV ECX, GuestECX

				// RDMSR
				_emit	0x0F
				_emit	0x32

				MOV GuestEAX, EAX
				MOV GuestEDX, EDX

				POPAD
			}

			// Done.
			break;


		case EXIT_REASON_MSR_WRITE:

			__asm
			{
				PUSHAD

				MOV ECX, GuestECX
				MOV EAX, GuestEAX
				MOV EDX, GuestEDX

				// WRMSR
				_emit	0x0F
				_emit	0x30

				POPAD
			}

			// Done.
			break;


		default:

			// Unhandled Exit Reason.
			hypervisor_panic();

			// Done.
			break;
	}

	// Restore the possibly updated state.
	GuestSTATE->GuestEAX = GuestEAX;
	GuestSTATE->GuestEBX = GuestEBX;
	GuestSTATE->GuestECX = GuestECX;
	GuestSTATE->GuestEDX = GuestEDX;
}
