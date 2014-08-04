/**
	@file
	Code for monitoring processes and splitting the TLB for target 
    applications
		
	@date 1/24/2012
***************************************************************/

#include "Ntifs.h"
#include "Wdm.h"
#include "ntddk.h"
#include "..\pe.h"
#include "..\paging.h"
#include "hypervisor_loader.h"
#include "ept.h"
#include "hypervisor.h"
#include "procmon.h"

/** Enable verbose debugging output */
#define VDEBUG 1
/** 8.3 target executable name */
char TargetAppName[] = "test.exe";

/** Stack storage for the APC state */
KAPC_STATE apcstate;
uint8 *appCopy = NULL;
/** MDL for probe and locking physical pages into memory */
PMDLX LockedMdl = NULL;
/** PHYSICAL_ADDRESS used to allow allocation anywhere in the 4GB range */
PHYSICAL_ADDRESS highestMemoryAddress = {0};
/** Pointer to the TlbTranslation array used by the EPT violation handler to split the TLB */
TlbTranslation *translationArr = NULL;

PHYSICAL_ADDRESS *targetPhys;

uint32 appsize = 0, targetCR3 = 0;

PHYSICAL_ADDRESS targetPePhys = {0};
void *targetPeVirt = NULL;
uint8 *targetPePtr = NULL;
PEPROCESS targetProc = NULL;
PageTableEntry **targetPtes;

/* Periodic Measurement Thread control (Created in entry, used in thread and unload) */
/** Thread object */
static VOID * periodicMeasureThread = NULL;
/** Thread body loop control variable */
static UCHAR periodicMeasureThreadExecute = 0;
/** Raise this event to abort the loop delay */
static KEVENT periodicMeasureThreadWakeUp = {0};

// This runs at a lower IRQL, so it can use the kernel memory functions
void processCreationMonitor(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create)
{
    PEPROCESS proc = NULL;
    void *PeHeaderVirt = NULL;
    uint16 numExecSections = 0;
    uint8 *pePtr = NULL;
    PHYSICAL_ADDRESS phys = {0};
    char *procName;
    uint32 imageSize, translations = (uint32) translationArr;
    
    NTSTATUS status = STATUS_SUCCESS;
    HANDLE periodMeasureThreadHandle = NULL;
    OBJECT_ATTRIBUTES objectAttributes = {0};
    
    // Set to anywhere inthe 4GB range
    highestMemoryAddress.LowPart = ~0;
    
    // Get the 8.3 image name
    PsLookupProcessByProcessId(ProcessId, &proc);
    procName = PsGetProcessImageFileName(proc);
    
    // Check if this is the target process
    if(strncmp(TargetAppName, procName, strlen(TargetAppName)) == 0)
    {
        if (Create && VDEBUG) DbgPrint("New Process Created! %s\r\n", procName); 
        if (!Create && VDEBUG) DbgPrint("Application quitting %s\r\n", procName);   
            
        // Retrieve virtual pointer to the PE header for target application (in PE context)
        PeHeaderVirt = PsGetProcessSectionBaseAddress(proc);
        //DbgPrint("Virt: %x", PeHeaderVirt);
        
        // Begin critical section
        // Attach to the target process and grab its CR3 value to use later
        KeStackAttachProcess(proc, &apcstate); 
        
        if (Create)
        {
            __asm
            {
                push eax
                mov eax, cr3
                mov targetCR3, eax
                pop eax
            }   
        }
        
        phys = MmGetPhysicalAddress(PeHeaderVirt);            
        KeUnstackDetachProcess(&apcstate);
        // End critical section
        
        targetPePhys = phys;
        targetPeVirt = PeHeaderVirt;
        targetProc = proc;
        
        if (Create)
        {
            targetPePtr = peMapInImageHeader(phys);
            imageSize = peGetImageSize(targetPePtr);
            if (VDEBUG) DbgPrint("Image Size: %x bytes Num Entries %d\r\n", imageSize, sizeof(TlbTranslation) * (imageSize / PAGE_SIZE));
            DbgPrint("Virt %x - %x %x\r\n", PeHeaderVirt, (uint32) PeHeaderVirt + imageSize, targetCR3);
        
            // Ensure Windows doesn't reuse the physical pages
            LockedMdl = pagingLockProcessMemory(PeHeaderVirt, imageSize, proc, &apcstate);
            if(LockedMdl == NULL && VDEBUG)
            {
                DbgPrint("Unable to lock memory\r\n");
            }
            appsize = imageSize;
            appCopy = (uint8 *) MmAllocateContiguousMemory(imageSize, highestMemoryAddress);
            RtlZeroMemory((void *) appCopy, imageSize);
            copyPe(proc, &apcstate, PeHeaderVirt, appCopy, imageSize);
            translationArr = allocateAndFillTranslationArray(PeHeaderVirt, 
                                                             appCopy, 
                                                             imageSize, 
                                                             proc, 
                                                             &apcstate);
                                                             
            translations = (uint32) translationArr;
            // VMCALL to start the TLB splitting
        	__asm
        	{
        		PUSHAD
        		MOV		EAX, VMCALL_INIT_SPLIT
                MOV     EBX, translations
        
        		_emit 0x0F		// VMCALL
        		_emit 0x01
        		_emit 0xC1
        
        		POPAD
        	}
            
            if (VDEBUG) DbgPrint("Checksum of proc: %x\r\n", 
                             peChecksumExecSections(targetPePtr, PeHeaderVirt, 
                                                    proc, &apcstate, targetPhys));
                             
            //pePrintSections(pePtr);
                             
#ifdef PERIODIC_MEASURE
            /* Set up periodic measurement thread */
            KeInitializeEvent(&periodicMeasureThreadWakeUp, NotificationEvent, FALSE); //returns void
            InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL); //returns void
        
            periodicMeasureThreadExecute = 1; //allows thread to execute
            status = PsCreateSystemThread(&periodMeasureThreadHandle,
                            THREAD_ALL_ACCESS, &objectAttributes, NULL, NULL,
                            periodicMeasurePe, NULL);
            status = ObReferenceObjectByHandle(periodMeasureThreadHandle, 0, NULL,
                                               KernelMode, &periodicMeasureThread, NULL);
            ZwClose(periodMeasureThreadHandle); //don't need the handle anymore, ref will remain valid
#endif

        }
        else
        {
            translations = (uint32) translationArr;
            // VMCALL to stop TLB splitting
        	__asm
        	{
        		PUSHAD
        		MOV		EAX, VMCALL_END_SPLIT
                MOV     EBX, translations
        
        		_emit 0x0F		// VMCALL
        		_emit 0x01
        		_emit 0xC1
        
        		POPAD
        	}
            if (LockedMdl != NULL)
            {
               pagingUnlockProcessMemory(proc, &apcstate, LockedMdl);
            }
            
            if (appCopy != NULL)
            {
                MmFreeContiguousMemory((PVOID) appCopy);
            }
            
            if (translationArr != NULL)
            {
                freeTranslationArray(translationArr);
            }

            targetCR3 = 0;
            
#ifdef PERIODIC_MEASURE
            /* Stop the periodic measurement thread */
            periodicMeasureThreadExecute = 0; // Apply brakes
            KeSetEvent(&periodicMeasureThreadWakeUp, 0, TRUE); // Cancel any current wait in the thread
            /* Wait for thread to stop */
            KeWaitForSingleObject(periodicMeasureThread, 
                                  Executive, 
                                  KernelMode, 
                                  FALSE, 
                                  NULL); 
        
            ObDereferenceObject(periodicMeasureThread);
#endif
            peMapOutImageHeader(targetPePtr);
            targetPeVirt = NULL;
        }
        return;        
    }
}

void periodicMeasurePe(PVOID context)
{
    LARGE_INTEGER measurementFrequency = {0}; // How long to delay

    measurementFrequency.QuadPart = -10000000; // 1 second / 100 nanoseconds
    while (periodicMeasureThreadExecute) {
        measurePe(targetPePhys, targetPeVirt);
        KeWaitForSingleObject(&periodicMeasureThreadWakeUp, Executive,
                              KernelMode, TRUE, &measurementFrequency); 
    }
}

void measurePe(PHYSICAL_ADDRESS phys, void * peHeaderVirt)
{
    uint32 b, c;
    if (peHeaderVirt == NULL)
        return;
    b = phys.LowPart;
    c = (uint32) peHeaderVirt;
    // VMCALL to stop measure without the EPT TLB splitting
	__asm
	{
		PUSHAD
		MOV		EAX, VMCALL_MEASURE
        MOV     EBX, b
        MOV     ECX, c

		_emit 0x0F		// VMCALL
		_emit 0x01
		_emit 0xC1

		POPAD
	}
    
}

void copyPe(PEPROCESS proc, PKAPC_STATE apc, uint8 *srcPtr, uint8 *targetPtr, uint32 len)
{
    if (srcPtr == NULL || targetPtr == NULL)
        return;
        
    // Attach to the process and copy the image to the passed buffer
    KeStackAttachProcess(proc, apc); 
    
    memcpy(targetPtr, srcPtr, len);
    
    KeUnstackDetachProcess(apc);
}

TlbTranslation * allocateAndFillTranslationArray(uint8 *codePtr,
                                                 uint8 *dataPtr, 
                                                 uint32 len, 
                                                 PEPROCESS proc,
                                                 PKAPC_STATE apc)
{
    const uint32 tag = '3gaT';
    uint32 i = 0, numPages = len / 0x1000;
    TlbTranslation *arr = (TlbTranslation *) ExAllocatePoolWithTag(NonPagedPool,
                                                 (numPages + 1) * sizeof(TlbTranslation),
                                                 tag);
                                                 
    PHYSICAL_ADDRESS tmpPhys = {0};
    TlbTranslation nullTranslation = {0};                                             
                                                 
    targetPhys = (PHYSICAL_ADDRESS *) ExAllocatePoolWithTag(NonPagedPool,
                                                 (numPages + 1) * sizeof(PHYSICAL_ADDRESS),
                                                 tag);
    targetPtes = (PageTableEntry **) ExAllocatePoolWithTag(NonPagedPool,
                                                 (numPages + 1) * sizeof(PageTableEntry *),
                                                 tag);                                             

    if (arr == NULL || targetPtes == NULL || targetPhys == NULL)
    {
        while (1) {};
    }
    
    RtlZeroMemory(arr, (numPages + 1) * sizeof(TlbTranslation));    
    // Loop through the VA space of the PE image and get the physical addresses
    for (i = 0; i < numPages; i++)
    {
        KeStackAttachProcess(proc, apc);
        tmpPhys = MmGetPhysicalAddress((PVOID) ((uint32) codePtr + (i * PAGE_SIZE)));
        KeUnstackDetachProcess(apc);
        targetPtes[i] = pagingMapInPte(targetCR3, (uint8 *) codePtr + (i * PAGE_SIZE));
        arr[i].CodePhys = tmpPhys.LowPart;
        targetPhys[i] = tmpPhys;
        //arr[i].DataPhys = tmpPhys.LowPart;
        arr[i].CodeOrData = CODE_EPT;
        
        arr[i].VirtualAddress = ((uint32) codePtr + (i * PAGE_SIZE));
        
        tmpPhys = MmGetPhysicalAddress((PVOID) ((uint32) dataPtr + (i * PAGE_SIZE)));
        arr[i].DataPhys = tmpPhys.LowPart;
        //arr[i].CodePhys = tmpPhys.LowPart;
        //DbgPrint("Code %x Data %x\r\n", arr[i].CodePhys, arr[i].DataPhys);
    }

    arr[numPages] = nullTranslation; // Zero out the last element
    return arr;
}

void freeTranslationArray(TlbTranslation *arr)
{
    const uint32 tag = '3gaT';
    uint32 i = 0;
    
    for (i = 0; i < appsize / 0x1000; i++)
    {
        pagingMapOutEntry(targetPtes[i]);
    }
    ExFreePoolWithTag(arr, tag);
}

TlbTranslation * getTlbTranslation(TlbTranslation * transArr, uint32 guestPhysical)
{
    uint32 i = 0;
    guestPhysical &= 0xFFFFF000;
    if (transArr == NULL)
        return NULL;
    // Look for the correct TlbTranslation
    while (transArr[i].DataPhys != 0)
    {
        if ((transArr[i].CodeOrData == DATA_EPT && guestPhysical == transArr[i].DataPhys) ||
                (transArr[i].CodeOrData == CODE_EPT && guestPhysical == transArr[i].CodePhys))
            return &transArr[i];
        i++;
    }
    return NULL;
}

// This function runs at DIRQL, and must NOT cause any page faults
// TODO Check for CodeOrData field
void AppendTlbTranslation(TlbTranslation * transArr, uint32 phys, uint8 * virt)
{
    uint32 i = 0;
    EptPteEntry *pte;
    while (transArr[i].VirtualAddress != (uint32) virt && transArr[i].DataPhys != 0)
    {
        i++;
    }
    if (transArr[i].VirtualAddress == (uint32) virt)
    {
        pte = transArr[i].EptPte;
        pte->Present = 1;
        pte->Write = 1;
        pte->Execute = 1;
        pte->PhysAddr = (transArr[i].CodeOrData == CODE_EPT) ? 
                            transArr[i].CodePhys >> 12 : 
                            transArr[i].DataPhys >> 12;
        transArr[i].DataPhys = phys;
        transArr[i].CodeOrData = DATA_EPT;
        pte = EptMapAddressToPteDirql(phys, NULL, &memContext);
        if (pte != NULL)
        {
            pte->Present = 0;
            pte->Write = 0;
            pte->Execute = 0;
            transArr[i].EptPte = pte;
        }
        else
        {
            //Beep(1);
            while(1) {}
        }
    }
    
}

uint8 *dataPage, *codePage;
TlbTranslation smallArr[2] = {0};
void splitPage()
{
    const uint32 tag = '3gaT';
    PHYSICAL_ADDRESS phys = {0};
    TlbTranslation *tlbptr = smallArr;
    
    dataPage = (uint8 *) ExAllocatePoolWithTag(NonPagedPool, 2 * PAGE_SIZE, tag);
    codePage = dataPage + PAGE_SIZE;
    
    dataPage[0] = 0xFF;
    codePage[0] = 0xC3;
    
    phys = MmGetPhysicalAddress((void *) dataPage);
    smallArr[0].DataPhys = phys.LowPart;
    phys = MmGetPhysicalAddress((void *) codePage);
    smallArr[0].CodePhys = phys.LowPart;
    
    __asm
	{
		PUSHAD
		MOV		EAX, VMCALL_INIT_SPLIT
        MOV     EBX, tlbptr

		_emit 0x0F		// VMCALL
		_emit 0x01
		_emit 0xC1

		POPAD
	}
    
    Log("Found", codePage[0]);
    codePage[0] = 0xFE;
    Log("Found", codePage[0]);
    
    __asm 
    {
        PUSH EAX
        MOV EAX, codePage
        CALL EAX
        POP EAX
    }
    
    __asm
	{
		PUSHAD
		MOV		EAX, VMCALL_END_SPLIT
        MOV     EBX, tlbptr

		_emit 0x0F		// VMCALL
		_emit 0x01
		_emit 0xC1

		POPAD
	}
    
    ExFreePoolWithTag(dataPage, tag);
}

uint32 checksumBuffer(uint8 * ptr, uint32 len)
{
    uint32 i, sum = 0;
    for (i = 0; i < len; i++)
    {
        sum += ptr[i];
    }
    return sum;
}
