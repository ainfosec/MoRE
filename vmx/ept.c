/**
	@file
	Code for handling EPT translations in a hypervisor
		
	@date 1/17/2012
***************************************************************/

#include "Ntifs.h"
#include "Wdm.h"
#include "ntddk.h"
#include "..\pe.h"
#include "procmon.h"
#include "ept.h"
#include "..\stack.h"
#include "..\paging.h"
#include "hypervisor_loader.h"
#include "hypervisor.h"

/** Number of EPT PDE pages to allocate, one per GB of memory */
#define NUM_PD_PAGES 4
/** Maximum number of EPT page tables */ 
#define NUM_TABLES 512

/** Pointer to the 512 PDPTEs covering the first 512GB of memory */
EptPdpteEntry *BkupPdptePtr = NULL;
/** Array of pointers to free for the PDEs */
EptPdeEntry2Mb *BkupPdePtrs[NUM_PD_PAGES] = {0};
uint32 EptPageTableCounter = 0, TableVirtsCounter = 0, ViolationExits = 0, 
                        ExecExits = 0, DataExits = 0, Thrashes = 0, Thrash = 0;
EptPteEntry *EptTableArray[NUM_TABLES] = {0};
EptPteEntry *EptTableVirts[NUM_TABLES] = {0};
TlbTranslation *splitPages = NULL;
uint8 ProcessorSupportsType0InvVpid = 0;
/** Stack to store faulting addresses for TLB split */
Stack pteStack = {0};
PagingContext memContext = {0};

void EnableEpt(uint32 Pml4Ptr)
{
    uint32 reg = ReadVMCS(SECONDARY_VM_EXEC_CONTROL);
    EptTablePointer EPTP = {0};
    PHYSICAL_ADDRESS phys = MmGetPhysicalAddress((void *) Pml4Ptr);
    
    // Set up the EPTP
    EPTP.Bits.PhysAddr = phys.LowPart >> 12;
    EPTP.Bits.PageWalkLength = 3;
    WriteVMCS(EPT_POINTER, EPTP.unsignedVal & 0xFFFFFFFF);
    WriteVMCS(EPT_POINTER_HIGH, 0);
    
    // Set the guest VPID to a non-zero value
    WriteVMCS(VIRTUAL_PROCESSOR_ID, VM_VPID);
    
    // Enable the secondary controls VMCS field
    WriteVMCS(SECONDARY_VM_EXEC_CONTROL, reg | (1 << 5) | (1 << 1) );
    reg = ReadVMCS(CPU_BASED_VM_EXEC_CONTROL);
    WriteVMCS(CPU_BASED_VM_EXEC_CONTROL, reg | (1 << 31));
    InvVpidAllContext();
}

void DisableEpt()
{
    uint32 reg = ReadVMCS(SECONDARY_VM_EXEC_CONTROL);
    WriteVMCS(SECONDARY_VM_EXEC_CONTROL, reg & ~((1 << 5) | (1 << 1)));
    
    // Clear out the EPTP
    WriteVMCS(EPT_POINTER, 0);
    WriteVMCS(EPT_POINTER_HIGH, 0);
}

EptPml4Entry * InitEptIdentityMap()
{
    EptPml4Entry *pml4Ptr = NULL;
    EptPdpteEntry *pdptePtr = NULL;
    PHYSICAL_ADDRESS phys = {0}, Highest = {0}, Lowest = {0};
    uint32 i, j, pdeCounter = 0;
    
    Highest.LowPart = ~0;
    
    // Allocate contiguous, un-cached memory
    pml4Ptr = (EptPml4Entry *) MmAllocateContiguousMemorySpecifyCache(
                                                    sizeof(EptPml4Entry) * 512, 
                                                    Lowest, 
                                                    Highest, 
                                                    Lowest, 
                                                    0);
    
    if (pml4Ptr == NULL)
    {
        return NULL;
    }
    
    pdptePtr = (EptPdpteEntry *) MmAllocateContiguousMemorySpecifyCache(
                                                    sizeof(EptPdpteEntry) * 512, 
                                                    Lowest, 
                                                    Highest, 
                                                    Lowest, 
                                                    0);
    // Save a copy of the virtual address for later freeing
    BkupPdptePtr = pdptePtr;
    if (pdptePtr == NULL)
    {
        MmFreeContiguousMemory(pml4Ptr);
        return NULL;
    }
    
    for (i = 0; i < NUM_PD_PAGES; i++)
    {
        BkupPdePtrs[i] = (EptPdeEntry2Mb *) MmAllocateContiguousMemorySpecifyCache(
                                                sizeof(EptPdeEntry2Mb) * 512, 
                                                Lowest, 
                                                Highest, 
                                                Lowest, 
                                                0);
        
        // Free memory if we fail to allocate the next chunk
        if (BkupPdePtrs[i] != NULL)
        {
            RtlZeroMemory((void *) BkupPdePtrs[i], sizeof(EptPdeEntry2Mb) * 512);
        } 
        else
        {
            MmFreeContiguousMemory(pml4Ptr);
            MmFreeContiguousMemory(pdptePtr);
            for (j = 0; j < i; j++)
            {
                MmFreeContiguousMemory(BkupPdePtrs[j]);
            }
            return NULL;
        }
    }
    
    phys = MmGetPhysicalAddress((void *) pdptePtr);
    
    // Zero out the pages
    RtlZeroMemory((void *) pml4Ptr, sizeof(EptPml4Entry) * 512);
    RtlZeroMemory((void *) pdptePtr, sizeof(EptPdpteEntry) * 512);
    
    // Populate our newly created EPT tables!
    // Only need the first PML4 Entry unless we have more than 512 GB of RAM
    pml4Ptr->Present = 1;
    pml4Ptr->Write = 1;
    pml4Ptr->Execute = 1;
    pml4Ptr->PhysAddr = phys.LowPart >> 12;
    
    // Establish an identity map
    for (i = 0; i < NUM_PD_PAGES; i++)
    {
        phys = MmGetPhysicalAddress((void *) BkupPdePtrs[i]);
        pdptePtr[i].Present = 1;
        pdptePtr[i].Write = 1;
        pdptePtr[i].Execute = 1;
        pdptePtr[i].PhysAddr = phys.LowPart >> 12;
        
        // Populate our 4GBs worth of PDEs
        for (j = 0; j < 512; j++)
        {
            BkupPdePtrs[i][j].Present = 1;
            BkupPdePtrs[i][j].Write = 1;
            BkupPdePtrs[i][j].MemoryType = EPT_MEMORY_TYPE_WB;
            BkupPdePtrs[i][j].Execute = 1;
            BkupPdePtrs[i][j].Size = 1;
            BkupPdePtrs[i][j].PhysAddr = pdeCounter;
            pdeCounter++;
        }
    }
    
    return pml4Ptr;
}

void FreeEptIdentityMap(EptPml4Entry * ptr)
{
    uint32 i;
    if (BkupPdptePtr != NULL) MmFreeContiguousMemory((void *) BkupPdptePtr);
    if (ptr != NULL) MmFreeContiguousMemory((void *) ptr);

    for (i = 0; i < NUM_PD_PAGES; i++)
    {
        if (NULL != BkupPdePtrs[i])
            MmFreeContiguousMemory(BkupPdePtrs[i]);
    }

    for (i = 0; i < EptPageTableCounter; i++)
    {
        if (NULL != (void *) EptTableArray[i])
            MmFreeContiguousMemory((void *) EptTableArray[i]);
    }
}

EptPteEntry * EptMapAddressToPte(uint32 guestPhysicalAddress, EptPml4Entry * pml4Ptr)
{
    return EptMapAddressToPteDirql(guestPhysicalAddress, pml4Ptr, NULL);
}

uint8 EptPtExists(uint32 guestPhysicalAddress)
{
    uint32 pdpteOff = ((guestPhysicalAddress >> 30) & 0x3),
              pdeOff = ((guestPhysicalAddress >> 21) & 0x1FF);
    EptPdeEntry2Mb *pde = NULL;
    
    // Map in correct PDE
    pde = BkupPdePtrs[pdpteOff];
    
    // Determine if this is mapping a large 2MB page or points to a page table    
    return !(pde[pdeOff].Size);
}

EptPteEntry * EptMapAddressToPteDirql(uint32 guestPhysicalAddress, 
                                      EptPml4Entry * pml4Ptr, 
                                      PagingContext * context)
{
    uint32 i, pdpteOff = ((guestPhysicalAddress >> 30) & 0x3),
              pdeOff = ((guestPhysicalAddress >> 21) & 0x1FF), 
              pteOff = ((guestPhysicalAddress >> 12) & 0x1FF);
    EptPdeEntry2Mb *pde = NULL;
    EptPdpteEntry *pdpte = NULL;
    EptPteEntry *retVal = NULL, *pageTable = NULL;
    PHYSICAL_ADDRESS phys = {0};
    
    // Map in correct PDE
    pde = BkupPdePtrs[pdpteOff];
    
    // Determine if this is mapping a large 2MB page or points to a page table    
    if (pde[pdeOff].Size == 1)
    {
        // Need to allocate a page table which replaces the 2MB PDE
        phys.LowPart = ~0;
        if (context == NULL)
            pageTable = (EptPteEntry *) MmAllocateContiguousMemory(
                                                sizeof(EptPteEntry) * 512, phys);
        else
            pageTable = (EptPteEntry *) pagingAllocPage(context);
                
        if (pageTable == NULL)
        {
            goto abort;
        }
        // Zero out the new page table
        RtlZeroMemory((void *) pageTable, sizeof(EptPteEntry) * 512);
        
        // Populate the page table
        for (i = 0; i < 512; i++)
        {
            pageTable[i].Present = 1;
            pageTable[i].Write = 1;
            pageTable[i].MemoryType = EPT_MEMORY_TYPE_WB;
            pageTable[i].Execute = 1;
            pageTable[i].PhysAddr = (((pde[pdeOff].PhysAddr << 21) & 0xFFFFFFFF) >> 12) + i;
        }
        
        pde[pdeOff].Size = 0;
        pde[pdeOff].IgnorePat = 0;
        pde[pdeOff].MemoryType = 0;
        
        phys = MmGetPhysicalAddress((void *) pageTable);
        ((EptPdeEntry *) pde)[pdeOff].PhysAddr = phys.LowPart >> 12;      
        
        EptTableVirts[TableVirtsCounter] = pageTable;
        TableVirtsCounter++;
        
        if (context == NULL)
        {
            EptTableArray[EptPageTableCounter] = pageTable;
            EptPageTableCounter++;
        }  
        
        return &pageTable[pteOff];
    }

    // Map in existing PTE to return
    for (i = 0; i < TableVirtsCounter; i++)
    {
        if (EptTableVirts[i][0].PhysAddr << 12 <= guestPhysicalAddress &&
                    EptTableVirts[i][511].PhysAddr << 12 >= guestPhysicalAddress)
        {
            return &EptTableVirts[i][pteOff];
        }
    }
    
    
  abort:
    return retVal;
}

void EptUnmapPte(EptPteEntry * ptr)
{
    EptUnmapPteDirql(ptr, NULL);
}

void EptUnmapPteDirql(EptPteEntry * ptr, PagingContext * context)
{
    if (ptr == NULL)
        return;
    MapOutMemory(context, (void *) ptr, sizeof(EptPteEntry));
}

void MapOutMemory(PagingContext * context, void * ptr, uint32 size)
{
    if (context == NULL)
        MmUnmapIoSpace(ptr, size);
}

void * MapInMemory(PagingContext * context, PHYSICAL_ADDRESS phys, uint32 size)
{
    return MmMapIoSpace(phys, size, 0);
}

void SetTrapFlag(uint8 value)
{
    uint32 eflags = ReadVMCS(GUEST_RFLAGS);
    if (value == 1)
    {
        WriteVMCS(GUEST_RFLAGS, eflags | (1 << 8));
    }
    else
    {
        WriteVMCS(GUEST_RFLAGS, eflags & ~(1 << 8));
    }
}

void exit_reason_dispatch_handler__exec_trap(struct GUEST_STATE * GuestSTATE)
{
    EptPteEntry *pteptr = NULL;
    TlbTranslation *translationPtr = NULL;
    // Check to see if this is a trap caused by the TLB splitting
    if (!StackIsEmpty(&pteStack))
    {
        translationPtr = (TlbTranslation *) StackPop(&pteStack);
        if (translationPtr != NULL)
            pteptr = translationPtr->EptPte;
            
        // Mark everything non-present
        if (pteptr != NULL)
        {
            pteptr->Present = 0;
            pteptr->Write = 0;
            pteptr->Execute = 0;
        }
        SetTrapFlag(0);
        if (Thrash)
        {
            InvVpidIndividualAddress(VM_VPID, translationPtr->VirtualAddress);
            if (StackPeek(&pteStack) == translationPtr)
            {
                StackPop(&pteStack);
            }
            else
            {
                translationPtr = (TlbTranslation *) StackPop(&pteStack);
                pteptr = translationPtr->EptPte;
                pteptr->Present = 0;
                pteptr->Write = 0;
                pteptr->Execute = 0;
                InvVpidIndividualAddress(VM_VPID, translationPtr->VirtualAddress);
            }
            Thrash = 0;
            //InvVpidAllContext();
        }
    }
    else
    {
        // @todo Re-inject this interrupt into the guest
        Beep(1);
        __asm
        {
            CLI
            HLT
        }
    }
}

void exit_reason_dispatch_handler__exec_ept(struct GUEST_STATE * GuestSTATE)
{
    uint32 guestPhysical = (ReadVMCS(GUEST_PHYSICAL_ADDRESS)),
           exitQualification = ReadVMCS(EXIT_QUALIFICATION),
           guestLinear = ReadVMCS(GUEST_LINEAR_ADDRESS); 
    EptPteEntry *pteptr = NULL;       
    TlbTranslation *translationPtr = getTlbTranslation(splitPages, guestPhysical);
    
    // This is a bad sign, it means that it cannot find the proper translation
    if (translationPtr == NULL)
    {
        end_split(splitPages);
        return;
    }
    
    // @todo Determine the root cause of the stack overflowing
    // Ensure that there is space on the stack
    if (StackIsFull(&pteStack))
        DbgPrint("Overflow!\r\n");
      
    // Get the faulting EPT PTE
    pteptr = translationPtr->EptPte;
    if (pteptr != NULL && pteptr->Present == 1 && pteptr->Execute == 1)
    {
        return;   
    }
    
    if (!StackIsEmpty(&pteStack) && (void *) translationPtr != StackPeek(&pteStack))
    {
        ((TlbTranslation *) StackPeek(&pteStack))->EptPte->Present = 1;
        ((TlbTranslation *) StackPeek(&pteStack))->EptPte->Write = 1;
        ((TlbTranslation *) StackPeek(&pteStack))->EptPte->Execute = 1;
    }
    StackPush(&pteStack, (void *) translationPtr);
    ViolationExits++;
    
    /*if (exitQualification & EPT_MASK_GUEST_LINEAR_VALID)
    {
        Log("Guest Linear Address", guestLinear);
        Log("Guest Physical", guestPhysical);
        Log("Guest EIP", GuestSTATE->GuestEIP);
        Log("----------------------------", 0);
    }*/
    
    if (StackNumEntries(&pteStack) >= 2) // Thrashing
    {
        PHYSICAL_ADDRESS phys = {0};
        uint8 *dataPtr, *codePtr;
        
        // Check to ensure there has been no instruction corruption
        if (KeGetCurrentIrql() <= DISPATCH_LEVEL)
        {
            phys.LowPart = translationPtr->DataPhys;
            dataPtr = (uint8 *) MmMapIoSpace(phys, PAGE_SIZE, 0);
            phys.LowPart = translationPtr->CodePhys;
            codePtr = (uint8 *) MmMapIoSpace(phys, PAGE_SIZE, 0);
            if (0 != memcmp(dataPtr + (GuestSTATE->GuestEIP & 0xFFF),
                            codePtr + (GuestSTATE->GuestEIP & 0xFFF), 
                            ReadVMCS(VM_EXIT_INSTRUCTION_LEN)))
            {
                memcpy(dataPtr + (GuestSTATE->GuestEIP & 0xFFF),
                        codePtr + (GuestSTATE->GuestEIP & 0xFFF), 
                        ReadVMCS(VM_EXIT_INSTRUCTION_LEN));
            }    
            MmUnmapIoSpace(dataPtr, PAGE_SIZE);
            MmUnmapIoSpace(codePtr, PAGE_SIZE);
        }
        else
        {
            //Beep(1);
        }
        Thrash = 1;
        Thrashes++;
        
        pteptr->PhysAddr = translationPtr->DataPhys >> 12;
        pteptr->Execute = 1;
        pteptr->Present = 1;
        pteptr->Write = 1;
    }
    else
    {
        if (exitQualification & EPT_MASK_DATA_EXEC) // Execute access
        {
            ExecExits++;
            pteptr->PhysAddr = translationPtr->CodePhys >> 12;
            //pteptr->PhysAddr = translationPtr->DataPhys >> 12;
            pteptr->Execute = 1;
        }
        else if (exitQualification & EPT_MASK_DATA_READ || 
                    exitQualification & EPT_MASK_DATA_WRITE) // Data access
        {
            DataExits++;
            pteptr->PhysAddr = translationPtr->DataPhys >> 12;
            //pteptr->PhysAddr = translationPtr->CodePhys >> 12;
            pteptr->Present = 1;
            pteptr->Write = 1;
        }
        else
        {
            // Violation that is neither data access nor instruction fetch
            //Beep(1);
            __asm
            {
                CLI
                HLT
            }
        }   
    }
    // Set the trap flag to force another VMEXIT to the trap handler
    SetTrapFlag(1);
    //InvEptAllContext();
    //InvVpidAllContext();
}

void init_split(TlbTranslation * arrPtr)
{
    uint32 i = 0;
    EptPteEntry *pte = NULL;
    
    // (Re)initialize counters and the stack
    splitPages = arrPtr;
    StackInitStack(&pteStack);
    
    ViolationExits = 0;
    DataExits = 0;
    ExecExits = 0;
    Thrashes = 0;
    Thrash = 0;
#ifdef SPLIT_TLB
    Log("Initializing TLB split", 0);
    // For all the defined target pages
    while(arrPtr[i].DataPhys != 0 && i < appsize / PAGE_SIZE)
    {
        // Determine which guest physical address is the one to be marked non-present 
        if (arrPtr[i].CodeOrData == CODE_EPT)
        {
            pte = EptMapAddressToPte(arrPtr[i].CodePhys, NULL);
        }
        else
        {
            pte = EptMapAddressToPte(arrPtr[i].DataPhys, NULL);
        }
        pte->Present = 0;
        pte->Write = 0;
        pte->Execute = 0;
        arrPtr[i].EptPte = pte;
        i++;
    }
    // Clear the TLB
    InvEptAllContext();
    InvVpidAllContext();
#endif
}

void end_split(TlbTranslation * arrPtr)
{
    uint32 i = 0;
    EptPteEntry *pte = NULL;
#ifdef SPLIT_TLB
    Log("Tear-down TLB split", 0);
    DbgPrint("%d Total Violations: %d Data and %d Exec %d Thrashes\r\n",
            ViolationExits, 
            DataExits, 
            ExecExits, 
            Thrashes);
    if (arrPtr != NULL)
    {
        while(arrPtr[i].DataPhys != 0 && i < appsize / PAGE_SIZE)
        {
            // Restore the identity map
            pte = arrPtr[i].EptPte;
            if (arrPtr[i].CodeOrData == CODE_EPT)
            {
                pte->PhysAddr = arrPtr[i].CodePhys >> 12;
            }
            else
            {
                pte->PhysAddr = arrPtr[i].DataPhys >> 12;
            }
            pte->Present = 1;
            pte->Write = 1;
            pte->Execute = 1;
            i++;
        }
        // Invalidate TLB
        InvEptAllContext();
        InvVpidAllContext();
    }
    else
    {
        //Beep(1);
    }
#endif
    splitPages = NULL;
    StackInitStack(&pteStack);
}

static void __invVpidAllContext(uint32 invtype, InvVpidDesc desc)
{
    __asm
    {
        PUSH EAX
        MOV EAX, invtype
        PUSH	desc.dwords.dword1
        PUSH	desc.dwords.dword2
        PUSH	desc.dwords.dword3
        PUSH	desc.dwords.dword4
        
        _emit 0x66      // INVVPID EAX, [ESP]
        _emit 0x0F
        _emit 0x38
        _emit 0x81
        _emit 0x04
        _emit 0x24
        
        ADD ESP, 16
        POP EAX
    }
}

void InvVpidAllContext()
{
    InvVpidDesc desc = {0};
    __invVpidAllContext(2, desc);
}

void InvVpidIndividualAddress(uint16 vpid, uint32 address)
{
    InvVpidDesc desc = {0};
    if (ProcessorSupportsType0InvVpid == 1) // Ensure the process supports this type
    {
        desc.bits.LinearAddress = address;
        desc.bits.Vpid = vpid;
        __invVpidAllContext(0, desc);
    }
    else
    {
        __invVpidAllContext(2, desc);
    }
}

void InvEptAllContext()
{
    __asm
    {
        PUSH EAX
        MOV EAX, 2
        // Set 128 bits of zeros
        PUSH	DWORD PTR 0
        PUSH	DWORD PTR 0
        PUSH	DWORD PTR 0
        PUSH	DWORD PTR 0
        
        _emit 0x66      // INVEPT EAX, [ESP]
        _emit 0x0F
        _emit 0x38
        _emit 0x80
        _emit 0x04
        _emit 0x24
        
        ADD ESP, 16
        POP EAX
    }
}
