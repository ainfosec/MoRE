/**
	@file
	Header which defines structures for handling EPT translations
		
	@date 1/17/2012
***************************************************************/

#ifndef _MORE_EPT_H_
#define _MORE_EPT_H_

#include "..\stdint.h"
#include "..\paging.h"
#include "procmon.h"
#include "structs.h"

/** Intel-defined EPT memory types */
enum EPT_MEMORY_TYPE_E
{
    EPT_MEMORY_TYPE_UC = 0,
    EPT_MEMORY_TYPE_WC = 1,
    EPT_MEMORY_TYPE_WT = 4,
    EPT_MEMORY_TYPE_WP = 5,
    EPT_MEMORY_TYPE_WB = 6,
};

typedef enum EPT_MEMORY_TYPE_E EPT_MEMORY_TYPE;

/** Boolean for whether or not to split the TLB */
#define SPLIT_TLB 1
/** Maximum addressing width for the processor */
#define PHYSICAL_ADDRESS_WIDTH 36
/** Guest VPID value (must be non-zero) */
#define VM_VPID 1

/** Number of pages to pre-allocate for later use */
#define NUM_PAGES_ALLOC 1024

extern uint32 ViolationExits, ExecExits, DataExits, Thrashes;
extern TlbTranslation *splitPages;
extern PagingContext memContext;
extern uint8 ProcessorSupportsType0InvVpid;

// Defines for parsing the EPT violation exit qualification
/** Bitmask for data read violation */
#define EPT_MASK_DATA_READ 0x1
/** Bitmask for data write violation */
#define EPT_MASK_DATA_WRITE (1 << 1)
/** Bitmask for data execute violation */
#define EPT_MASK_DATA_EXEC (1 << 2)
/** Bitmask for if the guest linear address is valid */
#define EPT_MASK_GUEST_LINEAR_VALID (1 << 7)

// Function declarations

/**
    Turns on EPT & VPID and configures the EPTP
    
    @param Pml4Ptr Physical address of the EPTPML4
*/
void EnableEpt(uint32 Pml4Ptr);

/**
    Disables EPT & VPID
*/
void DisableEpt();

/**
    Allocates and initializes an identity map for EPT
    
    @return Pointer to the EPTPML4 table
*/
EptPml4Entry * InitEptIdentityMap();

/**
    Frees the EPT identity map and any mapped page tables
    
    @param ptr Pointer to the EPT PML4 table
*/
void FreeEptIdentityMap(EptPml4Entry * ptr);

/**
    Invalidates all the VPID contexts
*/
void InvVpidAllContext();

/**
    Invalidates the VPID entry for a given linear address
    
    @param vpid VPID context
    @param address Linear address to flush
*/
void InvVpidIndividualAddress(uint16 vpid, uint32 address);

/**
    Invalidates all EPT translations
*/
void InvEptAllContext();

/**
    Returns the EPT PTE for a guest physical address, down-grading from PDE to PTEs if needed
    
    @note Assumes system has less than 512 GB of memory
    
    @param guestPhysicalAddress 32-bit address to get the PTE for
    @param pml4Ptr Optional pointer to EPT PML4 table (if not provided will use EPTP from VMCS)
    
    @return Pointer to the EPT PTE for the passed address, or NULL if error
*/
EptPteEntry * EptMapAddressToPte(uint32 guestPhysicalAddress, EptPml4Entry * pml4Ptr);

/**
    Returns the EPT PTE for a guest physical address, down-grading from PDE to PTEs if needed
    
    @note Assumes system has less than 512 GB of memory
    
    @param guestPhysicalAddress 32-bit address to get the PTE for
    @param pml4Ptr Optional pointer to EPT PML4 table (if not provided will use EPTP from VMCS)
    @param context Pointer to the paging context
    
    @return Pointer to the EPT PTE for the passed address, or NULL if error
*/
EptPteEntry * EptMapAddressToPteDirql(uint32 guestPhysicalAddress, 
                                      EptPml4Entry * pml4Ptr, 
                                      PagingContext * context);

/**
    Unmaps a PTE mapped in but does not free the page table
    
    @param ptr Pointer to the PTE
*/
void EptUnmapPte(EptPteEntry * ptr);

/**
    Unmaps a PTE mapped in but does not free the page table (at DIRQL)
    
    @param ptr Pointer to the PTE
    @param context Pointer to paging context
*/
void EptUnmapPteDirql(EptPteEntry * ptr, PagingContext * context);

/**
    VM Exit handler for EPT violation
    
    @param GuestSTATE State of the guest
*/
void exit_reason_dispatch_handler__exec_ept(struct GUEST_STATE * GuestSTATE);

/**
    VM Exit handler for the trap flag
    
    @param GuestSTATE State of the guest
*/
void exit_reason_dispatch_handler__exec_trap(struct GUEST_STATE * GuestSTATE);

/**
    Sets the guest's trap flag
    
    @param value 1 or 0 value to set the trap flag to
*/
void SetTrapFlag(uint8 value);

/**
    Sets up the environment to split the TLB
    
    @param arrPtr Pointer to a TlbTranslation array
*/
void init_split(TlbTranslation * arrPtr);

/**
    Stops splitting the TLB for a memory region
    
    @param arrPtr Pointer to a TlbTranslation array
*/
void end_split(TlbTranslation * arrPtr);

/**
    Helper function to intelligently map out memory
    
    @param context Pointer to paging context, if NULL, then the Win32 function is used
    @param ptr Pointer to region to be mapped out
    @param size Number of bytes in region, if context != NULL size is PAGE_SIZE
*/
void MapOutMemory(PagingContext * context, void * ptr, uint32 size);

/**
    Helper function to intelligently map in physical addresses
    
    @param context Pointer to paging context, if NULL, then the Win32 function is used
    @param phys Physical address to map in 
    @param size Number of bytes (if context != NULL, size is always PAGE_SIZE)
    @return Pointer to mapped-in region
*/
void * MapInMemory(PagingContext * context, PHYSICAL_ADDRESS phys, uint32 size);

/**
    Function to determine whether or not the passed guest physical is in a PT or a PD
    
    @param guestPhysicalAddress Guest Physical
    @return 1 if there is a PTE for the address, 0 if there is a PDE for it
*/
uint8 EptPtExists(uint32 guestPhysicalAddress);

/**
    Helper function to call the INVVPID instruction with the passed type & descriptor
    
    @param invtype INVVPID type
    @param desc INVVPID descriptor
*/
static void __invVpidAllContext(uint32 invtype, InvVpidDesc desc);

#endif
