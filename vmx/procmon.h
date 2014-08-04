/**
	@file
	Header which defines structures for monitoring processes
		
	@date 1/24/2012
***************************************************************/

#ifndef _MORE_PROCMON_H_
#define _MORE_PROCMON_H_

#include "..\stdint.h"
#include "..\paging.h"
#include "structs.h"

/** Boolean to monitor processes or not */
#define MONITOR_PROCS 1
/** Boolean for whether or not to periodically measure the binary */
#define PERIODIC_MEASURE 1
/** VMCALL code to initialize the TLB split */
#define VMCALL_INIT_SPLIT 0x100F
/** VMCALL code to end the TLB split */
#define VMCALL_END_SPLIT 0x200F
/** VMCALL code to measure the PE */
#define VMCALL_MEASURE 0x300F

#define DATA_EPT 0x1
#define CODE_EPT 0x2

/**
    Defines a structure to store the data and code page translations
*/
struct TlbTranslation_s
{
    uint32 VirtualAddress;
    uint32 DataPhys;
    uint32 CodePhys;
    uint8 CodeOrData;
    uint8 RW;
    EptPteEntry *EptPte;
};

typedef struct TlbTranslation_s TlbTranslation;

extern PVOID PsGetProcessSectionBaseAddress(PEPROCESS);
extern char * PsGetProcessImageFileName(PEPROCESS);

extern PHYSICAL_ADDRESS targetPePhys;
extern void *targetPeVirt;
extern uint8 *targetPePtr;
extern PEPROCESS targetProc;
extern KAPC_STATE apcstate;

extern uint32 targetCR3;

extern uint8 *appCopy;
extern uint32 appsize;

extern PageTableEntry **targetPtes;

extern PHYSICAL_ADDRESS *targetPhys;

/** Repeatedly calls measure */
static KSTART_ROUTINE periodicMeasurePe;

/**
    @brief Callback for when a new process is created
    
    Detects if the new process is a target for TLB splitting
    @param ParentID ID of parent process
    @param ProcessId ID of newly created process
    @param Create True if the process is being created, false if it's being destroyed
*/
void processCreationMonitor(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create);

/**
    Function to make a copy of a PE image
    
    @param proc PEPROCESS of the target PE
    @param apc Pointer to an APC state structure
    @param srcPtr Source VA
    @param targetPtr Memory buffer to copy to
    @param len Number of bytes to copy
*/
void copyPe(PEPROCESS proc, PKAPC_STATE apc, uint8 *srcPtr, uint8 *targetPtr, uint32 len);

/**
    Allocates and fills in an array (null terminated) of VA -> Physical mappings and PTEs
    
    @param srcPtr Pointer to image base
    @param targetPtr Pointer to copy
    @param len Number of bytes to copy
    @param cr3Reg CR3 register of target application
    @return Pointer to TlbTranslation array
*/
TlbTranslation * allocateAndFillTranslationArray(uint8 *codePtr,
                                                 uint8 *dataPtr, 
                                                 uint32 len, 
                                                 PEPROCESS proc,
                                                 PKAPC_STATE apc);

/**
    Frees and safely de-allocates a TLB translation array allocated with
    allocateAndFillTranslationArry

    @param arr Pointer to TlbTranslation array
*/
void freeTranslationArray(TlbTranslation *arr);

/**
    Returns the relevant TlbTranslation for the passed guest physial address
    
    @param transArr Pointer to array of all known translations
    @param guestPhysical Physical address
    
    @return Pointer to TlbTranslation pointing to the guest physical
*/
TlbTranslation * getTlbTranslation(TlbTranslation * transArr, uint32 guestPhysical);

#if 0
/** 
    Function to 'lock' a process' memory into physical memory and prevent paging
    
    @param startAddr Starting virtual address to lock
    @param len Number of bytes to lock
    @param proc PEPROCESS pointer to the process
    @param apcstate Pointer to an APC state memory location
    @return MDL to be used later to unlock the memory or NULL if lock failed
*/
PMDLX pagingLockProcessMemory(PVOID startAddr, uint32 len, PEPROCESS proc, PKAPC_STATE apcstate);

/** 
    Function to 'lock' a process' memory into physical memory and prevent paging
    
    @param proc PEPROCESS pointer to the process
    @param apcstate Pointer to an APC state memory location
    @param mdl Pointer to previously locked process MDL
*/
void pagingUnlockProcessMemory(PEPROCESS proc, PKAPC_STATE apcstate, PMDLX mdl);
#endif

/**
    Tests the TLB splitting for a single page instance
    This function emulates the functionality of MoRE drop 1
*/
void splitPage();

/**
    Measures the PE and displays the checksum
    
    @param phys Physical address of the image header
    @param PeHeaderVirt Process's virtual address
*/
void measurePe(PHYSICAL_ADDRESS phys, void * peHeaderVirt);

void AppendTlbTranslation(TlbTranslation * transArr, uint32 phys, uint8 * virt);

uint32 checksumBuffer(uint8 * ptr, uint32 len);

#endif
