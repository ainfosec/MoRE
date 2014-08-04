/**
	@file
	Windows driver to manipulate the page tables
		
	@date 11/17/2011
***************************************************************/
#include "Ntifs.h"
#include "ntddk.h"
#include "../stdint.h"
#include "paging.h"
#include "vmx/ept.h"

uint32 ContextTag = '2gaT';

void pagingMapOutEntry(void *ptr)
{
    pagingMapOutEntryDirql(ptr, NULL);
}

void pagingMapOutEntryDirql(void *ptr, PagingContext * context)
{
    if (ptr != NULL)
        MapOutMemory(context, ptr, sizeof(PageTableEntry));
}

PageTableEntry * pagingMapInPte(uint32 CR3, void *virtualAddress)
{
    return pagingMapInPteDirql(CR3, virtualAddress, NULL);
}

PageTableEntry * pagingMapInPteDirql(uint32 CR3, 
                                     void *virtualAddress, 
                                     PagingContext * context)
{
    PHYSICAL_ADDRESS pageTablePhys = {0};
    PageDirectoryEntrySmallPage *pageDirectory = (PageDirectoryEntrySmallPage *)
                                            pagingMapInPdeDirql(CR3, virtualAddress, context);
    PageTableEntry *outPte = NULL;
    uint32 pdeOff, pteOff, pageOff = 0;
    
    if (pageDirectory == NULL)
    {
        return NULL;
    }

    // Determine if we're dealing with large or small pages
    if (pageDirectory->ps == 1 || pageDirectory->p == 0)
    {
        // We are a PDE, not PTE   
        outPte = NULL;
    } 
    else
    {
        pteOff = ((uint32) virtualAddress & 0x003FF000) >> 12;
        pageTablePhys.LowPart = (pageDirectory->address << 12) | 
                                            (pteOff * sizeof(PageTableEntry));
                                            
        outPte = MapInMemory(context, pageTablePhys, sizeof(*outPte));
    }
    
    pagingMapOutEntryDirql((void *) pageDirectory, context);
    return outPte;
}

PageDirectoryEntry * pagingMapInPde(uint32 CR3, void *virtualAddress)
{
    return pagingMapInPdeDirql(CR3, virtualAddress, NULL);
}

PageDirectoryEntry * pagingMapInPdeDirql(uint32 CR3, 
                                         void *virtualAddress, 
                                         PagingContext * context)
{
    PHYSICAL_ADDRESS pageDirPhys = {0};
    uint32 pdeOff = ((uint32) virtualAddress & 0xFFC00000) >> 22;
    pageDirPhys.LowPart = (CR3 & 0xFFFFF000) | 
                                        (pdeOff * sizeof(PageDirectoryEntry));
    
    return (PageDirectoryEntry *) MapInMemory(context, pageDirPhys, 
                                            sizeof(PageDirectoryEntry));
}

PageWalkContext * pagingInitWalk(PHYSICAL_ADDRESS physicalAddress)
{
    PageWalkContext *context = (PageWalkContext *) ExAllocatePoolWithTag(
                                                        NonPagedPool, 
                                                        sizeof(PageWalkContext), 
                                                        ContextTag);
    PHYSICAL_ADDRESS pdeBase = {0};
    uint32 cr3val = 0;
    
    __asm
    {
        push eax
        mov eax, cr3
        mov cr3val, eax
        pop eax
    }
    
    pdeBase.LowPart = cr3val & 0xFFFFF000;
    
    context->targetAddress = physicalAddress;
    context->pdeOff = 0;
    context->pteOff = 0;
    context->pte = NULL;
    context->pde = MmMapIoSpace(pdeBase, 1024 * sizeof(PageDirectoryEntry), 0);
    
    return context;
}

uint32 pagingGetNext(PageWalkContext *context)
{
    PHYSICAL_ADDRESS ptePhys = {0};
    if (context->pde == NULL)
        return 0;
    if (context->pte == NULL)
    {
        for ( ; context->pdeOff < 1024; context->pdeOff++)
        {
            // Large pages (could be paged out)
            if ((context->pde[context->pdeOff]).ps == 1)
            {
                if (context->targetAddress.LowPart >> 22 == 
                                        (context->pde[context->pdeOff]).address)
                {
                    // Prevent an endless loop
                    context->pdeOff++;
                    return ((context->pdeOff - 1) << 22) | 
                                (context->targetAddress.LowPart & 0x003FFFFF);
                }
            }
            // Small pages
            else if ((context->pde[context->pdeOff]).p == 1 &&
                                        (context->pde[context->pdeOff]).ps == 0)
            {
                ptePhys.LowPart = ((PageDirectoryEntrySmallPage *) 
                                    &(context->pde[context->pdeOff]))->address << 12;
                context->pte = MmMapIoSpace(ptePhys, 1024 * sizeof(PageTableEntry), 0);
                
                // Loop through a page table
                for (context->pteOff = 0; context->pteOff < 1024; context->pteOff++)
                {
                    if ((context->pte[context->pteOff]).address ==
                                            context->targetAddress.LowPart >> 12)
                    {
                        //DbgPrint("Address %x\r\n", (context->pte[context->pteOff]).address);
                        context->pteOff++;
                        return (context->pdeOff << 22) | ((context->pteOff - 1) << 12) |
                                (context->targetAddress.LowPart & 0x00000FFF);
                    }
                }
                
                MmUnmapIoSpace(context->pte, 1024 * sizeof(PageTableEntry));
                context->pte = NULL;
                context->pteOff = 0;
            }
        }
    }
    else
    {
        // Loop through a page table
        for ( ; context->pteOff < 1024; context->pteOff++)
        {
            if ((context->pte[context->pteOff]).address ==
                                    context->targetAddress.LowPart >> 12)
            {
                //DbgPrint("Address %x\r\n", (context->pte[context->pteOff]).address);
                context->pteOff++;
                return (context->pdeOff << 22) | ((context->pteOff - 1) << 12) |
                        (context->targetAddress.LowPart & 0x00000FFF);
            }
        }
        
        MmUnmapIoSpace(context->pte, 1024 * sizeof(PageTableEntry));
        context->pte = NULL;
        context->pteOff = 0;
        context->pdeOff++;
        
        // Recurse to the next PDE
        return pagingGetNext(context);
    }
    
    // Not found
    return 0;
}

void pagingFreeWalk(PageWalkContext *context)
{
    if (context->pde != NULL)
    {
        MmUnmapIoSpace(context->pde, 1024 * sizeof(*(context->pde)));
    }
    if (context->pte != NULL)
    {
        MmUnmapIoSpace(context->pte, 1024 * sizeof(*(context->pte)));
    }
    
    ExFreePoolWithTag((PVOID) context, ContextTag);
}

PMDLX pagingLockProcessMemory(PVOID startAddr, 
                              uint32 len,
                              PEPROCESS proc, 
                              PKAPC_STATE apcstate)
{
    PMDLX mdl = NULL;
    
    // Attach to process to ensure virtual addresses are correct
    KeStackAttachProcess(proc, apcstate); 
    
    // Create MDL to represent the image
    mdl = IoAllocateMdl(startAddr, (ULONG) len, FALSE, FALSE, NULL);
    if (mdl == NULL)
        return NULL;
    
    // Attempt to probe and lock the pages into memory
    try 
    {
        MmProbeAndLockPages(mdl, UserMode, ReadAccess);
    } except (EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("Unable to ProbeAndLockPages! Error: %x\r\n", GetExceptionCode());
        
        IoFreeMdl(mdl);
        mdl = NULL;
    }
    
    KeUnstackDetachProcess(apcstate);
    
    
    return mdl;
}

void pagingUnlockProcessMemory(PEPROCESS proc, PKAPC_STATE apcstate, PMDLX mdl)
{
    // Attach to process to ensure virtual addresses are correct
    KeStackAttachProcess(proc, apcstate); 
    
    // Unlock & free MDL and corresponding pages
    MmUnlockPages(mdl);
    IoFreeMdl(mdl);
    
    KeUnstackDetachProcess(apcstate);
}

void pagingInitMappingOperations(PagingContext *context, uint32 numPages)
{
    uint32 i, cr3Val;
    const uint32 tag = '4gaT';
    PHYSICAL_ADDRESS phys = {0};
    PageDirectoryEntrySmallPage *pde;
    
    __asm
    {
        PUSH EAX
        MOV EAX, CR3
        MOV cr3Val, EAX
        POP EAX
    }
    context->CR3Val = cr3Val;
    phys.LowPart = cr3Val & 0xFFFFF000;
    
    context->PageArray = (uint8 *) ExAllocatePoolWithTag(NonPagedPool, 
                                                    numPages * PAGE_SIZE, tag);
    context->NumPages = numPages;
    context->PageArrayBitmap = (uint8 *) ExAllocatePoolWithTag(NonPagedPool, 
                                                                numPages, tag);
    RtlZeroMemory(context->PageArrayBitmap, numPages);

}

void pagingEndMappingOperations(PagingContext *context)
{
    const uint32 tag = '4gaT';
    PHYSICAL_ADDRESS phys = {0};
    PageDirectoryEntrySmallPage *pde;

    ExFreePoolWithTag(context->PageArray, tag);
    context->NumPages = 0;
    ExFreePoolWithTag(context->PageArrayBitmap, tag);
    
}

void * pagingAllocPage(PagingContext *context)
{
    uint32 i;
    for (i = 0; i < context->NumPages; i++)
    {
        if (context->PageArrayBitmap[i] == 0)
        {
            // Mark page as taken
            context->PageArrayBitmap[i] = 1;
            return context->PageArray + (i * PAGE_SIZE);
        }
    }
    // No memory left
    return NULL;
}

void pagingFreePage(PagingContext *context, void * ptr)
{
    // Mark that page as free
    context->PageArrayBitmap[((uint8 *) ptr - context->PageArray) / PAGE_SIZE] = 0;
}
