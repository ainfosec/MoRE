#ifndef __HYPERVISOR_H
#define __HYPERVISOR_H

#include "vmcs.h"
#include "index.h"

// ================================================================================
// Exit dispatch typedef

// The follwing defines an exit reason dispatch handler. This
// tells the compiler what an exit reason dispatch handler looks
// like. This is used so that arrays of these handlers can be
// supported while also supporting the use of arguments.

typedef void ( * exit_reason_dispatch_handler ) ( struct GUEST_STATE * GuestSTATE );

// ================================================================================
// Read / Write VMCS

unsigned int ReadVMCS( unsigned int encoding );
void WriteVMCS( unsigned int encoding, unsigned int value );

// ================================================================================
// Logging Macro.

#define Log( message, value ) { DbgPrint("[exec] %-40s [%08X]\n", message, value ); }

// ================================================================================
// Load / Unload functions.

void load_hypervisor( void );
void unload_hypervisor( void );

// ================================================================================
// Exit Dispatch Handlers.

void exit_reason_dispatch_handler__exec_vmcall( struct GUEST_STATE * GuestSTATE );
void exit_reason_dispatch_handler__exec_cra( struct GUEST_STATE * GuestSTATE );

// ================================================================================
// Entry Point

void hypervisor_entry_point( void );
void advance_eip( void );
void hypervisor_exit_handler( void );
void hypervisor_panic( void );
void disable_exec_hypervisor( void );

#endif

