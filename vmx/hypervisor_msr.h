#ifndef __HYPERVISOR_MSR_H
#define __HYPERVISOR_MSR_H

// ================================================================================
// Define an MSR Slot

struct MSR_SLOT
{
	unsigned int set;
	unsigned int msr_num;
};

// ================================================================================
// Define the init/exit routines for this root command module.

void mc__init( void );
void mc__exit( void );

// ================================================================================
// Exit Dispatch Handlers.

void exit_reason_dispatch_handler__msr_vmcall( struct GUEST_STATE * GuestSTATE );
void exit_reason_dispatch_handler__msr_msr( struct GUEST_STATE * GuestSTATE );

#endif
