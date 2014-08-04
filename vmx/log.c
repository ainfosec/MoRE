#include "log.h"

// ================================================================================
// Define global variables.

// Control Register Access.
struct log_store log_cra__cr3 = {0};
struct log_store log_cra__rdtsc = {0};

// ================================================================================
// Define the init routine for this root command module.

void lc__init( void )
{

}

void lc__exit( void )
{
	// Save the log stores.
	log_store__save( &log_cra__cr3, L"\\DosDevices\\C:\\log_cra__cr3.txt" );
	log_store__save( &log_cra__rdtsc, L"\\DosDevices\\C:\\log_cra__rdtsc.txt" );

	// Unallocate all of the log stores.
	log_store__unallocate( &log_cra__cr3 );
	log_store__unallocate( &log_cra__rdtsc );
}

// ================================================================================
// Define the log store functions.

void log_store__allocate( struct log_store * log_store_reg, unsigned int size )
{
	// Unallocate the buffer if it has already been allocated.
	log_store__unallocate( log_store_reg );

	// Allocate memory for the buffer.
	log_store_reg->buffer = MmAllocateNonCachedMemory( size * sizeof( unsigned int ) );

	// Clear the index. The index is used to count through the buffer,
	// since this is a new buffer, the index needs to be cleared.
	log_store_reg->index = 0x0;

	// Store the size of the buffer. This tells the log function when to stop.
	log_store_reg->size = size;

	// Tell the log store that its buffer is allocated.
	log_store_reg->allocated = 0x1;

	// Tell the log store that its not finished logging.
	log_store_reg->finished = 0x0;
}

void log_store__unallocate( struct log_store * log_store_reg )
{
	// Check to see if the log store has already been allocated. If it has
	// delete the allocation.
	if ( log_store_reg->allocated )
	{
		MmFreeNonCachedMemory( log_store_reg->buffer, log_store_reg->size * sizeof( unsigned int ) );
	}

	// Clear the index.
	log_store_reg->index = 0x0;

	// Clear the size.
	log_store_reg->size = 0x0;

	// Tell the log store that its buffer is unallocated.
	log_store_reg->allocated = 0x0;

	// Clear the finished flag.
	log_store_reg->finished = 0x0;
}

void log_store__save( struct log_store * log_store_reg, PCWSTR filename )
{
	// Define the local variables.
	UNICODE_STRING		uniFilename;
	OBJECT_ATTRIBUTES	objAttr;
	HANDLE			handle;
	NTSTATUS		ntstatus;
	IO_STATUS_BLOCK		ioStatusBlock;

	// Store the filename
	RtlInitUnicodeString( &uniFilename, filename );
	InitializeObjectAttributes( &objAttr, &uniFilename, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL );

	// Create the file.
	ntstatus = ZwCreateFile( &handle,
				 GENERIC_WRITE,
				 &objAttr,
				 &ioStatusBlock,
				 NULL,
				 FILE_ATTRIBUTE_NORMAL,
				 0,
				 FILE_OVERWRITE_IF,
				 FILE_SYNCHRONOUS_IO_NONALERT,
				 NULL,
				 0 );

	// If the file was created, save the contents of the buffer.
	if( NT_SUCCESS( ntstatus ) )
	{
		// Write to the file.
		ntstatus = ZwWriteFile( handle,
				        NULL,
				        NULL,
				        NULL,
				        &ioStatusBlock,
				        ( char * ) log_store_reg->buffer,
				        log_store_reg->size * sizeof( unsigned int ),
				        NULL,
				        NULL );

		// Close the file.
		ZwClose( handle );
	}
	else
	{
		DbgPrint( "Error Creating File\n" );
	}


}

void log_store__log( struct log_store * log_store_reg, unsigned int data, char * msg )
{
	// If the log store is allocated, and index is not overrun,
	// log the cr3
	if ( log_store_reg->allocated && log_store_reg->index < log_store_reg->size )
	{
		// Store the CR3 value.
		log_store_reg->buffer[log_store_reg->index] = data;

		// Increment the index.
		log_store_reg->index++;
	}

	// Send message about finishing. To do this, check to make sure the
	// buffer is allcoated, and it has run out, and make sure we have
	// not already sent this message.
	if ( log_store_reg->allocated && log_store_reg->index >= log_store_reg->size && log_store_reg->finished == 0x0 )
	{
		DbgPrint( msg );

		// Do not show this again.
		log_store_reg->finished = 0x1;
	}
}

// ================================================================================
// Dispatch handlers.

// VMCall
void exit_reason_dispatch_handler__log_vmcall ( struct GUEST_STATE * GuestSTATE )
{
	// Check the guest state to make sure the command sent belongs
	// to this module.
	if ( GuestSTATE->GuestEAX == rc_eax__log )
	{
		// Switch the exit reason.
		switch( GuestSTATE->GuestEBX )
		{
			case lc_ebx__cra:

//				DbgPrint( "Log Command: Control Register Access\n" );
//				DbgPrint( "    - GuestEAX: 0x%x\n", GuestSTATE->GuestEAX );
//				DbgPrint( "    - GuestEBX: 0x%x\n", GuestSTATE->GuestEBX );
//				DbgPrint( "    - GuestECX: 0x%x\n", GuestSTATE->GuestECX );
//				DbgPrint( "    - GuestEDX: 0x%x\n", GuestSTATE->GuestEDX );
//				DbgPrint( "    - GuestESI: 0x%x\n", GuestSTATE->GuestESI );
//				DbgPrint( "    - GuestEDI: 0x%x\n", GuestSTATE->GuestEDI );

				// Allocate the log stores.
				log_store__allocate( &log_cra__cr3, GuestSTATE->GuestECX );
				log_store__allocate( &log_cra__rdtsc, GuestSTATE->GuestECX );

				// Done.
				break;

			default:

				// Done.
				break;
		}

	}
}

// Control Register Access.
void exit_reason_dispatch_handler__log_cra ( struct GUEST_STATE * GuestSTATE )
{
	// ------------------------------------------------------------------------
	// CR3

	{
		// Log
		log_store__log( &log_cra__cr3, ReadVMCS( GUEST_CR3 ), "LOG: CRA Finished (CR3)\n" );
	}

	// ------------------------------------------------------------------------
	// RDTSC

	{
		// Store the lower 32 bits of the rdtsc instruction.
		unsigned int rdtsc_lo = 0x0;

		// Get the lower 32 bits of the rdtsc instruction.
		__asm
		{
			PUSHAD

			// Read the time-stamp counter.
			rdtsc

			// Store the result (lower 32 bits only).
			mov rdtsc_lo, eax;

			POPAD
		}

		// Log
		log_store__log( &log_cra__rdtsc, rdtsc_lo, "LOG: CRA Finished (RDTSC)\n" );
	}
}
