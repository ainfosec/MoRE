/**
	@file
	Defines PE format structures
		
	@date 11/10/2011
***************************************************************/

#ifndef _MORE_PE_H_
#define _MORE_PE_H_

#include "stdint.h"

// Bitmask defines
/** The section contains executable code */
#define IMAGE_SCN_CNT_CODE 0x00000020
/** The section contains initialized data */
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
/** The section contains un-initialized data */
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
/** The section is executable */
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
/** The section is readable */
#define IMAGE_SCN_MEM_READ 0x40000000
/** The section is writable */
#define IMAGE_SCN_MEM_WRITE 0x80000000

/** Defines a high-low relocation type */
#define IMAGE_REL_BASED_HIGHLOW 3
/** Defines an absolute relocation type (unused by PE loader) */
#define IMAGE_REL_BASED_ABSOLUTE 0

#pragma pack(push, pe, 1)

/**
    Structure representing the DOS stub of a PE
    
    @note Known in MS docs as IMAGE_DOS_HEADER
*/
struct ImageDosHeader_s
{
    uint16 e_magic;
    uint16 e_cblp;
    uint16 e_cp;
    uint16 e_crlc;
    uint16 e_cparhdr;
    uint16 e_minalloc;
    uint16 e_maxalloc;
    uint16 e_ss;
    uint16 e_sp;
    uint16 e_csum;
    uint16 e_ip;
    uint16 e_cs;
    uint16 e_lfarlc;
    uint16 e_ovno;
    uint16 e_res[4];
    uint16 e_oemid;
    uint16 e_oeminfo;
    uint16 e_res2[10];
    uint32 e_lfanew;
};

typedef struct ImageDosHeader_s ImageDosHeader;

/**
    Structure representing a PE file header
    
    @note Known as IMAGE_FILE_HEADER in MS docs
*/
struct ImageFileHeader_s
{
    uint16 Machine;
    uint16 NumberOfSections;
    uint32 TimeDateStamp;
    uint32 PointerToSymbolTable;
    uint32 NumberOfSymbols;
    uint16 SizeOfOptionalHeader;
    uint16 Characteristics;
};

typedef struct ImageFileHeader_s ImageFileHeader;

/**
    Structure representing a PE data directory
    
    @note Known as IMAGE_DATA_DIRECTORY in MS docs
*/
struct ImageDataDirectory_s
{
    uint32 VirtualAddress;
    uint32 Size;
};

typedef struct ImageDataDirectory_s ImageDataDirectory;

/**
    Structure representing a PE optional header
    
    @note Known as IMAGE_OPTIONAL_HEADER in MS docs
    @note DataDirectory can have more or less entries than 16
*/
struct ImageOptionalHeader_s
{
    uint16 Magic;
    uint8 MajorLinkerVersion;
    uint8 MinorLinkerVersion;
    uint32 SizeOfCode;
    uint32 SizeOfInitializedData;
    uint32 SizeOfUninitializedData;
    uint32 AddressOfEntryPoint;
    uint32 BaseOfCode;
    uint32 BaseOfData;
    uint32 ImageBase;
    uint32 SectionAlignment;
    uint32 FileAlignment;
    uint16 MajorOperatingSystemVersion;
    uint16 MinorOperatingSystemVersion;
    uint16 MajorImageVersion;
    uint16 MinorImageVersion;
    uint16 MajorSubsystemVersion;
    uint16 MinorSubsystemVersion;
    uint32 Win32VersionValue;
    uint32 SizeOfImage;
    uint32 SizeOfHeaders;
    uint32 CheckSum;
    uint16 Subsystem;
    uint16 DllCharacteristics;
    uint32 SizeOfStackReserve;
    uint32 SizeOfStackCommit;
    uint32 SizeOfHeapReserve;
    uint32 SizeOfHeapCommit;
    uint32 LoaderFlags;
    uint32 NumberOfRvaAndSizes;
    ImageDataDirectory DataDirectory[16];
};

typedef struct ImageOptionalHeader_s ImageOptionalHeader;

/**
    Structure representing a PE NT header
    
    @note Known as IMAGE_NT_HEADERS in MS docs
*/
struct ImageNtHeaders_s
{
    uint32 Signature;
    ImageFileHeader FileHeader;
    ImageOptionalHeader OptionalHeader;
};

typedef struct ImageNtHeaders_s ImageNtHeaders;

/**
    Struct representing the PE relocation header
    
    @note Known as IMAGE_BASE_RELOCATION in MS docs
*/
struct ImageBaseRelocation_s
{
    uint32 VirtualAddress;
    uint32 SizeOfBlock;
};

typedef struct ImageBaseRelocation_s ImageBaseRelocation;

/**
    Structure for a PE relocation
*/
struct PeRelocation_s
{
    uint16 RelocationOffset :12;
    uint16 RelocationType :4;
};

typedef struct PeRelocation_s PeRelocation;

/**
    Structure representing a PE section header
    
    @note Known as IMAGE_SECTION_HEADER in MS docs
*/
struct ImageSectionHeader_s
{
    uint8 Name[8];
    union
    {
        uint32 PhysicalAddress;
        uint32 VirtualSize;
    } Misc;
    uint32 VirtualAddress;
    uint32 SizeOfRawData;
    uint32 PointerToRawData;
    uint32 PointerToRelocations;
    uint32 PointerToLinenumbers;
    uint16 NumberOfRelocations;
    uint16 NumberOfLinenumbers;
    uint32 Characteristics; /**< Bitmask of section characteristics */
};

typedef struct ImageSectionHeader_s ImageSectionHeader;

// Reuse ImageDataDirectory structure for section data
typedef ImageDataDirectory SectionData;

#pragma pack(pop, pe)

/**
    Returns the number of relocations in the section
    
    @param headerPtr Pointer to the relocation table header
    @param realBase The virtual address the PE is loaded into
    @param proc Pointer to the EPROCESS for the target process
    @param apc Pointer to an APC state storage location
    @return Number of relocations in the table
*/
uint32 peGetNumberOfRelocs(uint8 *peBaseAddr, void *realBase, PEPROCESS proc, PKAPC_STATE apc);

/**
    Calculates the 'delta' between the linked and loaded address for 
    relocations
    
    @param peBaseAddr Pointer to mapped in PE structure
    @param realBase The virtual address the PE is loaded into
    @return Difference between the linked and loaded executable
*/
static uint32 peCalculateRelocDiff(uint8 *peBaseAddr, void *realBase);

/**
    Returns the number of bytes in the PE image
    
    @param peBaseAddr Pointer to the base image address
    @return Number of bytes in PE image
*/
uint32 peGetImageSize(uint8 *peBaseAddr);

/**
    Maps a PE header into memory from a physical address
    
    @note Ensure memory is mapped out with peMapOutImage
    @param physAddr Physical address of PE base
    @return Pointer to mapped in PE image header
*/
uint8 * peMapInImageHeader(PHYSICAL_ADDRESS physAddr);

/**
    Unmaps a PE image from virtual memory
    
    @param peBaseAddr Pointer to the base image address
*/
void peMapOutImageHeader(uint8 *peBaseAddr);

/**
    Prints the sections found in a PE
    
    @param peBaseAddr Pointer to the base image address
*/
void pePrintSections(uint8 *peBaseAddr);

/**
    Returns the number of executable sections in the PE image
    
    @param peBaseAddr Pointer to the base image address
    @return Number of sections marked as executable
*/
uint16 peGetNumExecSections(uint8 *peBaseAddr);

/**
    Populates the passed SectionData array with pointers and sizes to each 
    executable section of the PE
    
    @note The sections array must be allocated by caller using the number
    returned by peGetNumExecSections
    @param peBaseAddr Pointer to the base image address
    @param sections Pointer to array memory location to store the section data
*/
void peGetExecSections(uint8 *peBaseAddr, SectionData *sections);

/**
    Returns a simple checksum of all the executable sections of the passed PE
    
    @param peBaseAddr Pointer to the base image address
    @param realBase The real base address mapped in by the loader
    @param proc Pointer to the EPROCESS for the PE
    @param apc Pointer to an APC state storage location
    @return Simple checksum of the executable sections of the PE
*/
uint32 peChecksumExecSections(uint8 *peBaseAddr, void *realBase, PEPROCESS proc, PKAPC_STATE apc, PHYSICAL_ADDRESS *physArr);

/**
    Returns a simple checksum of all the executable sections of the passed PE using a different physical mapping
    
    @param peBaseAddr Pointer to the base image address
    @param realBase The real base address mapped in by the loader
    @param proc Pointer to the EPROCESS for the PE
    @param apc Pointer to an APC state storage location
    @param physArr Array of physical addresses to use instead of what is in the paging structures
    @return Simple checksum of the executable sections of the PE
*/
uint32 peChecksumBkupExecSections(uint8 *peBaseAddr, void *realBase, PEPROCESS proc, PKAPC_STATE apc, PHYSICAL_ADDRESS *physArr);

#endif  // _MORE_PE_H
