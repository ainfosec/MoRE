#ifndef __LOG_H
#define __LOG_H

#include "ntddk.h"

#include "hypervisor.h"

// Define a log store. This is used to store all the needed information 
// about a log of any exit reason. Keep in mind that not all log data is 
// stored for all logs. This can be selective. 

struct log_store
{
	unsigned int index;
	unsigned int * buffer;
	unsigned int size;
	unsigned int allocated;
	unsigned int finished;
};

// ================================================================================
// Define the init/exit routines for this root command module.

void lc__init( void );
void lc__exit( void );

// ================================================================================
// Dispatch handlers.

void exit_reason_dispatch_handler__log_vmcall ( struct GUEST_STATE * GuestSTATE );
void exit_reason_dispatch_handler__log_cra ( struct GUEST_STATE * GuestSTATE );

// ================================================================================
// Define the log store functions.

void log_store__allocate( struct log_store * log_store_reg, unsigned int size );
void log_store__unallocate( struct log_store * log_store_reg );
void log_store__save( struct log_store * log_store_reg, PCWSTR filename );
void log_store__log( struct log_store * log_store_reg, unsigned int data, char * msg );

#endif
