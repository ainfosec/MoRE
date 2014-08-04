/**
	@file
	Header which defines structures for handling EPT translations
		
	@date 2/1/2012
***************************************************************/

#ifndef _MORE_STRUCTS_H_
#define _MORE_STRUCTS_H_

#pragma pack(push, ept, 1)

struct EptTablePointer_s
{
    uint64 MemoryType :3; // EPT Paging structure memory type (0 for UC)
    uint64 PageWalkLength :3; // Page-walk length - 1
    uint64 reserved1 :6; // Reserved
    uint64 PhysAddr :24; // Physical address of the EPT PML4 table
    uint64 reserved2 :28;
};

union EptTablePointer_u
{
    uint64 unsignedVal;
    struct EptTablePointer_s Bits;
};

typedef union EptTablePointer_u EptTablePointer;

struct EptPml4Entry_s
{
    uint64 Present :1; // If the 512 GB region is present (read access)
    uint64 Write :1; // If the 512 GB region is writable
    uint64 Execute :1; // If the 512 GB region is executable
    uint64 reserved1 :9; // Reserved
    uint64 PhysAddr :24; // Physical address
    uint64 reserved2 :28; // Reserved
};

typedef struct EptPml4Entry_s EptPml4Entry;

struct EptPdpteEntry1Gb_s
{
    uint64 Present :1; // If the 1 GB region is present (read access)
    uint64 Write :1; // If the 1 GB region is writable
    uint64 Execute :1; // If the 1 GB region is executable
    uint64 MemoryType :3; // EPT Memory type
    uint64 IgnorePat :1; // Flag for whether to ignore PAT
    uint64 Size :1; // Must be 1
    uint64 reserved1 :22; // Reserved
    uint64 PhysAddr :6; // Physical address
    uint64 reserved2 :28; // Reserved
};

typedef struct EptPdpteEntry1Gb_s EptPdpteEntry1Gb;

struct EptPdpteEntry_s
{
    uint64 Present :1; // If the 1 GB region is present (read access)
    uint64 Write :1; // If the 1 GB region is writable
    uint64 Execute :1; // If the 1 GB region is executable
    uint64 reserved1 :9; // Reserved
    uint64 PhysAddr :24; // Physical address
    uint64 reserved2 :28; // Reserved
};

typedef struct EptPdpteEntry_s EptPdpteEntry;

struct EptPdeEntry_s
{
    uint64 Present :1; // If the 2 MB region is present (read access)
    uint64 Write :1; // If the 2 MB region is writable
    uint64 Execute :1; // If the 2 MB region is executable
    uint64 reserved1 :9; // Reserved
    uint64 PhysAddr :24; // Physical address
    uint64 reserved2 :28; // Reserved
};

typedef struct EptPdeEntry_s EptPdeEntry;

struct EptPdeEntry2Mb_s
{
    uint64 Present :1; // If the 1 GB region is present (read access)
    uint64 Write :1; // If the 1 GB region is writable
    uint64 Execute :1; // If the 1 GB region is executable
    uint64 MemoryType :3; // EPT Memory type
    uint64 IgnorePat :1; // Flag for whether to ignore PAT
    uint64 Size :1; // Must be 1
    uint64 reserved1 :13; // Reserved
    uint64 PhysAddr :15; // Physical address
    uint64 reserved2 :28; // Reserved
};

typedef struct EptPdeEntry2Mb_s EptPdeEntry2Mb;

struct EptPteEntry_s
{
    uint64 Present :1; // If the 1 GB region is present (read access)
    uint64 Write :1; // If the 1 GB region is writable
    uint64 Execute :1; // If the 1 GB region is executable
    uint64 MemoryType :3; // EPT Memory type
    uint64 IgnorePat :1; // Flag for whether to ignore PAT
    uint64 reserved1 :5; // Reserved
    uint64 PhysAddr :24; // Physical address
    uint64 reserved2 :28; // Reserved
};

typedef struct EptPteEntry_s EptPteEntry;

struct InvVpidDesc_s
{
    uint64 Vpid :16; // VPID to effect
    uint64 reserved :48; // Reserved
    uint64 LinearAddress :64; // Linear address
};

struct uint128_s
{
    uint32 dword1;
    uint32 dword2;
    uint32 dword3;
    uint32 dword4;
};

union InvVpidDesc_u
{
    struct uint128_s dwords;
    struct InvVpidDesc_s bits;
};

typedef union InvVpidDesc_u InvVpidDesc;

#pragma pack(pop, ept)

#endif
