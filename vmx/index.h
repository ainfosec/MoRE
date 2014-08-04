#ifndef __INDEX_H
#define __INDEX_H

// ================================================================================
// Shared Root Commands
//
// Note: The root command is indexed using eax.

#define rc_eax__exec	1
#define rc_eax__msr	2
#define rc_eax__log	3
#define rc_eax__smm	4

// Define the number of supported root commands
#define RC_EAX__NUM	5

#define rc_name__msr	"msr"
#define rc_name__log	"log"
#define rc_name__smm	"smm"
#define rc_help__msr	"MSR Root Command"
#define rc_help__smm	"Log Root Command"
#define rc_help__log	"SMM Root Command"

// ================================================================================
// Shared Log Definitions.

#define lc_ebx__cra 	1

// Define the number of supported exit reasons.
#define LC_EBX__NUM 	2

#define lc_name__cra	"cra"
#define lc_help__cra	"Control Register Access"

// ================================================================================
// Shared SMM Definitions.

#define sc_ebx__mem	1
#define sc_ebx__pio	2

// Define the number of supported smm vmcalls.
#define SC_EBX__NUM 	3

#define sc_name__mem	"mem"
#define sc_name__pio	"pio"
#define sc_help__mem	"Ignore Memory Instruction"
#define sc_help__pio	"Ignore Port IO Instruction"

// ================================================================================
// Shared MSR Definitions.

#define mc_ebx__rd	1
#define mc_ebx__wr	2

// Define the number of supported smm vmcalls.
#define MC_EBX__NUM 	3

#define mc_name__rd	"read"
#define mc_name__wr	"write"
#define mc_help__rd	"Protect MSR Reads (Currently Not Supported)"
#define mc_help__wr	"Protect MSR Writes"

#endif
