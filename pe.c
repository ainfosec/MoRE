/**
	@file
	Library to parse & measure PE files
		
	@date 11/14/2011
***************************************************************/
#include "ntifs.h"
#include "Wdm.h"
#include "ntddk.h"
#include "stdint.h"
#include "pe.h"
#include "paging.h"
#include "vmx/procmon.h"

uint32 peGetNumberOfRelocs(uint8 *peBaseAddr, void *realBase, PEPROCESS proc, PKAPC_STATE apc)
{
    ImageDosHeader *dosHeader = NULL;
    ImageNtHeaders *ntHeaders = NULL;
    ImageSectionHeader *sectionHeader = NULL;
    ImageBaseRelocation *relocationPtr = NULL, *bkupRPtr = NULL;
    uint32 numRelocs = 0;
    PageTableEntry *pte = NULL;
    PHYSICAL_ADDRESS phys = {0};
    
    uint16 i, j = 0, execSectionCount = 0, numSections = 0;
    
    dosHeader = (ImageDosHeader *) peBaseAddr;
    ntHeaders = (ImageNtHeaders *) ((uint8 *) peBaseAddr + dosHeader->e_lfanew);
    numSections = ntHeaders->FileHeader.NumberOfSections;
    sectionHeader = (ImageSectionHeader *) &ntHeaders[1];
    
    for (i = 0; i < numSections; i++)
    {
        if(strncmp(sectionHeader[i].Name, ".reloc", 8) == 0)
            break;
    }
    if(strncmp(sectionHeader[i].Name, ".reloc", 8) != 0)
            return 0;
    /*DbgPrint("Found %.08s RVA: %x Characteristics: %x", sectionHeader[i].Name, 
                                                        sectionHeader[i].VirtualAddress,
                                                        sectionHeader[i].Characteristics);*/

    //KeStackAttachProcess(proc, apc);
    pte = pagingMapInPte(targetCR3, (uint8 *) (((uint32) realBase) +
                                sectionHeader[i].VirtualAddress));
    if (pte == NULL)
        return 0;
    phys.LowPart = pte->address << 12;
    pagingMapOutEntry(pte);

    relocationPtr = (ImageBaseRelocation *) MmMapIoSpace(phys,
            PAGE_SIZE,
            0);
    bkupRPtr = relocationPtr;        
    /*DbgPrint("%p + %x = %x", realBase, sectionHeader[i].VirtualAddress, (((uint32) realBase) +
                                sectionHeader[i].VirtualAddress));*/
    i = 0;
    do
    {       
        //DbgPrint("RP: %x %x\r\n", relocationPtr->VirtualAddress, relocationPtr->SizeOfBlock); 
        numRelocs += (relocationPtr->SizeOfBlock - sizeof(*relocationPtr)) / sizeof(uint16);
        relocationPtr = (ImageBaseRelocation *) ((uint8 *) relocationPtr + relocationPtr->SizeOfBlock);
        i++;
    } while(relocationPtr->SizeOfBlock != 0);
    MmUnmapIoSpace(bkupRPtr, PAGE_SIZE);
    //KeUnstackDetachProcess(apc);
   //DbgPrint("I %d\r\n", i);
   
    // Size of the table (minus the header) divided by the size of each entry
    // FIXME Figure out why this is the case
    return numRelocs - (i);
}

static uint32 peCalculateRelocDiff(uint8 *peBaseAddr, void *realBase)
{
    ImageDosHeader *dosHeader = NULL;
    ImageNtHeaders *ntHeaders = NULL;
    uint16 *ptr = (uint16 *) peBaseAddr;
    uint32 imageBase = 0x01000000;
    
    dosHeader = (ImageDosHeader *) peBaseAddr;
    ntHeaders = (ImageNtHeaders *) ((uint8 *) peBaseAddr + dosHeader->e_lfanew);
    // Uncomment for a driver
    //imageBase = ntHeaders->OptionalHeader.ImageBase;
    
    if(((uint32) realBase) > imageBase)
        return ((uint32) realBase) - imageBase;
    return imageBase - ((uint32) realBase);
}

uint32 peGetImageSize(uint8 *peBaseAddr)
{
    ImageDosHeader *dosHeader = NULL;
    ImageNtHeaders *ntHeaders = NULL;
    uint16 *ptr = (uint16 *) peBaseAddr;
    
    dosHeader = (ImageDosHeader *) peBaseAddr;
    ntHeaders = (ImageNtHeaders *) ((uint8 *) peBaseAddr + dosHeader->e_lfanew);

    return ntHeaders->OptionalHeader.SizeOfImage;
}

uint8 * peMapInImageHeader(PHYSICAL_ADDRESS physAddr)
{
    uint8 *pePtr = NULL;
    uint32 imageSize = 0;

    pePtr = MmMapIoSpace(physAddr, PAGE_SIZE, 0);
    if (pePtr == NULL || *pePtr != 'M' || *(pePtr + 1) != 'Z')
    {
        DbgPrint("Invalid physical address!");
        if (pePtr != NULL)
            MmUnmapIoSpace(pePtr, PAGE_SIZE);
        return NULL;
    }
    
    return pePtr;
}

void peMapOutImageHeader(uint8 *peBaseAddr)
{
    MmUnmapIoSpace(peBaseAddr, PAGE_SIZE);
}

uint16 peGetNumExecSections(uint8 *peBaseAddr)
{
    ImageDosHeader *dosHeader = NULL;
    ImageNtHeaders *ntHeaders = NULL;
    ImageSectionHeader *sectionHeader = NULL;
    
    uint16 i, execSectionCount = 0, numSections = 0;
    
    dosHeader = (ImageDosHeader *) peBaseAddr;
    ntHeaders = (ImageNtHeaders *) ((uint8 *) peBaseAddr + dosHeader->e_lfanew);
    numSections = ntHeaders->FileHeader.NumberOfSections;

    sectionHeader = (ImageSectionHeader *) (&ntHeaders[1]);
    
    for (i = 0; i < numSections; i++)
    {
        if (sectionHeader[i].Characteristics & IMAGE_SCN_MEM_EXECUTE &&
            !(strcmp("INIT", sectionHeader[i].Name) == 0)) execSectionCount++;
    }
    
    return execSectionCount;
}

void peGetExecSections(uint8 *peBaseAddr, SectionData *sections)
{
    ImageDosHeader *dosHeader = NULL;
    ImageNtHeaders *ntHeaders = NULL;
    ImageSectionHeader *sectionHeader = NULL;
    
    uint16 i, j = 0, execSectionCount = 0, numSections = 0;
    
    dosHeader = (ImageDosHeader *) peBaseAddr;
    ntHeaders = (ImageNtHeaders *) ((uint8 *) peBaseAddr + dosHeader->e_lfanew);
    numSections = ntHeaders->FileHeader.NumberOfSections;
    sectionHeader = (ImageSectionHeader *) &ntHeaders[1];
    
    for (i = 0; i < numSections; i++)
    {
        if (sectionHeader[i].Characteristics & IMAGE_SCN_MEM_EXECUTE &&
                            !(strcmp("INIT", sectionHeader[i].Name) == 0))
        {
            sections[j].VirtualAddress = sectionHeader[i].VirtualAddress;
            sections[j].Size = sectionHeader[i].Misc.VirtualSize;
            //DbgPrint("Section %.8s is executable", sectionHeader[i].Name);
            j++;
        }
    }
}

void pePrintSections(uint8 *peBaseAddr)
{
    ImageDosHeader *dosHeader = NULL;
    ImageNtHeaders *ntHeaders = NULL;
    ImageSectionHeader *sectionHeader = NULL;
    
    uint16 i, j = 0, execSectionCount = 0, numSections = 0;
    
    dosHeader = (ImageDosHeader *) peBaseAddr;
    ntHeaders = (ImageNtHeaders *) ((uint8 *) peBaseAddr + dosHeader->e_lfanew);
    numSections = ntHeaders->FileHeader.NumberOfSections;
    sectionHeader = (ImageSectionHeader *) &ntHeaders[1];
    
    for (i = 0; i < numSections; i++)
    {
        DbgPrint("Section %d: %.8s VA: %x Size %x", i + 1, sectionHeader[i].Name,
                sectionHeader[i].VirtualAddress, sectionHeader[i].Misc.VirtualSize);
    }
}

uint32 peChecksumExecSections(uint8 *peBaseAddr, 
                              void *realBase, 
                              PEPROCESS proc, 
                              PKAPC_STATE apc, 
                              PHYSICAL_ADDRESS *physArr)
{
    uint16 numExecSections = peGetNumExecSections(peBaseAddr);
    uint32 checksum = 0, k, i, j, 
                        numRelocs = peGetNumberOfRelocs(peBaseAddr, realBase, proc, apc), 
                        relocDelta = peCalculateRelocDiff(peBaseAddr, realBase);
    uint8 *dataPtr = NULL;
    PHYSICAL_ADDRESS phys = {0};
    SectionData *execSections = (SectionData *) MmAllocateNonCachedMemory(
                                                numExecSections * sizeof(SectionData));
    peGetExecSections(peBaseAddr, execSections);
    
    //DbgPrint("Found %d relocations, delta of: %x\r\n", numRelocs, relocDelta);
    
    for (i = 0; i < numExecSections; i++)
    {   
        uint32 numpages = execSections[i].Size / 0x1000, size = execSections[i].Size;
        if (numpages * 0x1000 < execSections[i].Size)
            numpages++;
        for (k = 0; k < numpages; k++)
        {
            KeStackAttachProcess(proc, apc); 
            dataPtr = (uint8 *) MmMapIoSpace(MmGetPhysicalAddress((void *)(((uint32) realBase) +
                        execSections[i].VirtualAddress + (0x1000 * k))),
                        0x1000, 0);
            phys = MmGetPhysicalAddress((void *) dataPtr);

            for (j = 0; j < min(size, 0x1000); j++)
            {
                checksum += dataPtr[j];
            }
            MmUnmapIoSpace((void *) dataPtr, 0x1000);
            size -= 0x1000;
            KeUnstackDetachProcess(apc);
        }
    }
    
    // Subtract the relocations from the checksum
    // TODO Fix incase of lower load address
    checksum += numRelocs * (relocDelta & 0x000000FF);
    checksum += numRelocs * ((relocDelta & 0x0000FF00) >> 8);
    checksum += numRelocs * ((relocDelta & 0x00FF0000) >> 16);
    checksum += numRelocs * ((relocDelta & 0xFF000000) >> 24);
    
    
    MmFreeNonCachedMemory((void *) execSections, numExecSections * sizeof(SectionData));
    return checksum;
}

uint32 peChecksumBkupExecSections(uint8 *peBaseAddr, 
                                  void *realBase, 
                                  PEPROCESS proc, 
                                  PKAPC_STATE apc, 
                                  PHYSICAL_ADDRESS *physArr)
{
    uint16 numExecSections = peGetNumExecSections(peBaseAddr);
    uint32 checksum = 0, k, i, j, 
                        numRelocs = peGetNumberOfRelocs(peBaseAddr, realBase, proc, apc), 
                        relocDelta = peCalculateRelocDiff(peBaseAddr, realBase);
    uint8 *dataPtr = NULL;
    PHYSICAL_ADDRESS phys = {0};
    SectionData *execSections = (SectionData *) MmAllocateNonCachedMemory(
                                                numExecSections * sizeof(SectionData));
    peGetExecSections(peBaseAddr, execSections);
    
    //DbgPrint("Found %d relocations, delta of: %x\r\n", numRelocs, relocDelta);
    
    for (i = 0; i < numExecSections; i++)
    {   
        uint32 numpages = execSections[i].Size / 0x1000, size = execSections[i].Size;
        if (numpages * 0x1000 < execSections[i].Size)
            numpages++;
        for (k = 0; k < numpages; k++)
        {
            dataPtr = (uint8 *) MmMapIoSpace(physArr[(execSections[i].VirtualAddress / PAGE_SIZE) + k],
                        min(size, 0x1000), 0);
            for (j = 0; j < min(size, 0x1000); j++)
            {
                checksum += dataPtr[j];
            }
            MmUnmapIoSpace((void *) dataPtr, min(size, 0x1000));
            size -= 0x1000;
        }
    }
    
    // Subtract the relocations from the checksum
    // TODO Fix incase of lower load address
    checksum += numRelocs * (relocDelta & 0x000000FF);
    checksum += numRelocs * ((relocDelta & 0x0000FF00) >> 8);
    checksum += numRelocs * ((relocDelta & 0x00FF0000) >> 16);
    checksum += numRelocs * ((relocDelta & 0xFF000000) >> 24);
    
    
    MmFreeNonCachedMemory((void *) execSections, numExecSections * sizeof(SectionData));
    return checksum;
}
