/**
    @file
    CLI app to get/set PE section header characteristics 

    @date 12/13/2011
************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "pemod.h"

/*************************************/
// PRECOMPILER DIRECTIVES
/*************************************/

#define _PEMOD_MAKE_EXE_ //** Compiles as executable instead of library */

/*************************************/
// RUN-TIME LINKED FUNCTIONS
/*************************************/

/**
    From MSDN: Computes the checksum of the specified file.

    @param Filename IN PCSTR containing the file name for the file which the checksum is to be computed
    @param HeaderSum OUT PDWORD contains the checksum originally with file Filename
    @param CheckSum OUT PDWORD contains the newly computed checksum for the file Filename
    @return DWORD containing status value. CHECKSUM_SUCCESS = 0 indicates success
*/
typedef DWORD (__cdecl *_DLL_FN_MapFileAndCheckSumA) (PCSTR, PDWORD, PDWORD);
_DLL_FN_MapFileAndCheckSumA MapFileAndCheckSumA;

/*************************************/
// CONSTANTS
/*************************************/

/* MZ/PE Header Offsets */
const unsigned char kMzHeaderOffsetForPeHeaderOffset = 0x3C;
const unsigned char kPeHeaderOffsetForNumberOfSections = 0x6;
const unsigned char kSectionHeaderOffsetForCharacteristics = 0x24;
const unsigned char kPeSectionHeaderLength = 0x28;
const unsigned char kPeHeaderOffsetForCheckSum = 0x58;
const unsigned char kPeHeaderOffsetForStartOfSectionHeaders = 0xF8;

/*************************************/
// FUNCTION DEFINITIONS
/*************************************/

void showUsage()
{
    printf("\nThis program takes four arguments:\n");
    printf(" - A PE file to modify\n");
    printf(" - The PE section header to modify eg '.text'\n");
    printf(" - A case insensitive command 'get', 'set', 'unset', or 'replace'\n");
    printf(" - (if not using get) A hex dword with the flags to be set e.g. 'E0000040'\n");
    printf("These inputs will be used to modify all matching section header characteristics\n");
    printf("as well as update the checksum for the modified PE file.\n");
}


#ifdef _PEMOD_MAKE_EXE_
/**
    Program entry point.

    @param argc integer containing number of entries in argv[]
    @param argv array of character arrays (c-strings) containing arguments
    @return int containing status value, 0 is success, nonzero is failure.
*/
int __cdecl main(int argc, char ** argv)
{
    uint8 status = 0;
    uint32 characteristics = 0;

    //check argument count
    if (argc >= 4) {
        if (0 == _stricmp(argv[3], "get")) {
            if (argc != 4) {
                printf("!!! Invalid argument count\n");
                showUsage();
                return 1;
            }
        }
        else if (5 == argc) {
            sscanf_s(argv[4], "%X", &characteristics);
        }
        else {
            printf("!!! Invalid argument count\n");
            showUsage();
            return 1;

        }
    }
    else {
        printf("!!! Invalid argument count\n");
        showUsage();
        return 1;
    }

    //check command argument and perform command
    if (0 == _stricmp(argv[3], "get")) {
        status = getPeSectionCharacteristics(argv[1], argv[2], &characteristics);
        switch (status) {
            case 0:
                printf("Section Characteristics: %08X\n", characteristics);
                return 0;
                break;
            case 1:
                printf("!!! Error accessing PE file\n");
                showUsage();
                return 1;
                break;
            case 2:
                printf("!!! Unable to find specified section header\n");
                showUsage();
                return 1;
                break;
            default:
                printf("!!! Internal error G2. This is a bug.\n");
                break;
         }
        
    }
    else if (0 == _stricmp(argv[3], "set")) {
        status = setPeSectionCharacteristicsFlags(argv[1], argv[2], characteristics);
        switch (status) {
            case 0:
                printf("Success!\n");
                return 0;
                break;
            case 1:
                printf("!!! Error accessing PE file\n");
                showUsage();
                return 1;
                break;
            case 2:
                printf("!!! Unable to find specified section header\n");
                showUsage();
                return 1;
                break;
            default:
                printf("!!! Internal error S2. This is a bug.\n");
                break;
        }
    }
    else if (0 == _stricmp(argv[3], "unset")) {
        status = unsetPeSectionCharacteristicsFlags(argv[1], argv[2], characteristics);
        switch (status) {
            case 0:
                printf("Success!\n");
                return 0;
                break;
            case 1:
                printf("!!! Error accessing PE file\n");
                showUsage();
                return 1;
                break;
            case 2:
                printf("!!! Unable to find specified section header\n");
                showUsage();
                return 1;
                break;
            default:
                printf("!!! Internal error U2. This is a bug.\n");
                break;
        }
    }
    else if (0 == _stricmp(argv[3], "replace")) {
        status = setPeSectionCharacteristics(argv[1], argv[2], characteristics);
        switch (status) {
            case 0:
                printf("Success!\n");
                return 0;
                break;
            case 1:
                printf("!!! Error accessing PE file\n");
                showUsage();
                return 1;
                break;
            case 2:
                printf("!!! Unable to find specified section header\n");
                showUsage();
                return 1;
                break;
            default:
                printf("!!! Internal error R2. This is a bug.\n");
                break;
        }
    }

    printf("!!! ERROR: Invalid command parameter specified\n");
    showUsage();
    return 1;

}
#endif //_PEMOD_MAKE_EXE_

uint8 getPeSectionCharacteristics(IN char * peFile, IN char * sectionName,
                                  OUT uint32 *characteristics)
{

    DWORD peHeaderOffset = 0;
    WORD numberOfSections = 0;
    LONG sectionOffset = 0;
    FILE *peFileHandle = NULL;
    char sectionNameExactBytes[9] = {'\000'};
    ULONG i = 0; //index reused within various loops

    if (!peFile) {
        return 1;
    }
    if (!sectionName) {
        return 2;
    }
    if (!characteristics) {
        return 3;
    }

    for (i=0; (i < 8) && (sectionName[i] != '\0'); ++i) {
        sectionNameExactBytes[i] = sectionName[i];
    }

    if (!(peFileHandle = fopen(peFile, "r+b"))) {
        return 1;
    }
    if (0 != fseek(peFileHandle, kMzHeaderOffsetForPeHeaderOffset, SEEK_SET)) {
        fclose(peFileHandle);
        return 1;
    }
    for (i = 0; i < 4; ++i) {
        ((UCHAR *)(&peHeaderOffset))[i] = (UCHAR) fgetc(peFileHandle);
    }

    if (0 != fseek(peFileHandle,
            ((long)peHeaderOffset)+kPeHeaderOffsetForNumberOfSections,
            SEEK_SET)) {
        fclose(peFileHandle);
        return 1;
    }
    for (i = 0; i < 2; ++i) {
        ((UCHAR *)(&numberOfSections))[i] = (UCHAR) fgetc(peFileHandle);
    }

    for (sectionOffset = ((long)peHeaderOffset) +
            kPeHeaderOffsetForStartOfSectionHeaders; numberOfSections--;
            sectionOffset += kPeSectionHeaderLength) {
        if (0 != fseek(peFileHandle, sectionOffset, SEEK_SET)) {
            fclose(peFileHandle);
            return 1;
        }
        for (i=0; i < 8; ++i) {
            if (((UCHAR)(sectionNameExactBytes[i])) != fgetc(peFileHandle)) {
                break;
            }
        }

        if (8 == i) {
            if (0 != fseek(peFileHandle,
                    sectionOffset+kSectionHeaderOffsetForCharacteristics,
                    SEEK_SET)) {
                fclose(peFileHandle);
                return 1;
            }   
            //unrolled loop
            ((unsigned char *)characteristics)[0] = (unsigned char)fgetc(peFileHandle);
            ((unsigned char *)characteristics)[1] = (unsigned char)fgetc(peFileHandle);
            ((unsigned char *)characteristics)[2] = (unsigned char)fgetc(peFileHandle);
            if (!(((unsigned char *)characteristics)[3] = (unsigned char)fgetc(peFileHandle))) {
                //this read will return EOF if it, or ones before it reached EOF
                fclose(peFileHandle);
                return 1;
            }
            break;
        }

    }
    fclose(peFileHandle);

    if (8 != i) {
        return 2; 
    }

    return 0;
}

uint8 setPeSectionCharacteristics(IN char * peFile, IN char * sectionName,
                                  IN uint32 characteristics)
{

    DWORD peHeaderOffset = 0;
    WORD numberOfSections = 0;
    LONG sectionOffset = 0;
    FILE *peFileHandle = NULL;
    char sectionNameExactBytes[9] = {'\000'};
    ULONG i = 0; //index reused within various loops
    uint8 foundMatch = 0;

    if (!peFile) {
        return 1;
    }
    if (!sectionName) {
        return 2;
    }

    for (i=0; (i < 8) && (sectionName[i] != '\0'); ++i) {
        sectionNameExactBytes[i] = sectionName[i];
    }

    if (!(peFileHandle = fopen(peFile, "r+b"))) {
        return 1;
    }
    if (0 != fseek(peFileHandle, kMzHeaderOffsetForPeHeaderOffset, SEEK_SET)) {
        fclose(peFileHandle);
        return 1;
    }
    for (i = 0; i < 4; ++i) {
        ((UCHAR *)(&peHeaderOffset))[i] = (UCHAR) fgetc(peFileHandle);
    }

    if (0 != fseek(peFileHandle,
            ((long)peHeaderOffset)+kPeHeaderOffsetForNumberOfSections,
            SEEK_SET)) {
        fclose(peFileHandle);
        return 1;
    }
    for (i = 0; i < 2; ++i) {
        ((UCHAR *)(&numberOfSections))[i] = (UCHAR) fgetc(peFileHandle);
    }

    for (sectionOffset = ((long)peHeaderOffset) +
            kPeHeaderOffsetForStartOfSectionHeaders; numberOfSections--;
            sectionOffset += kPeSectionHeaderLength) {
        if (0 != fseek(peFileHandle, sectionOffset, SEEK_SET)) {
            fclose(peFileHandle);
            return 1;
        }
        for (i=0; i < 8; ++i) {
            if (((UCHAR)(sectionNameExactBytes[i])) != fgetc(peFileHandle)) {
                break;
            }
        }

        if (8 == i) {
            if (0 != fseek(peFileHandle,
            sectionOffset+kSectionHeaderOffsetForCharacteristics,
            SEEK_SET)) {
                fclose(peFileHandle);
                return 1;
            }   
            //unrolled loop
            fputc(((unsigned char *)&characteristics)[0],peFileHandle);
            fputc(((unsigned char *)&characteristics)[1],peFileHandle);
            fputc(((unsigned char *)&characteristics)[2],peFileHandle);
            if (EOF == fputc(((unsigned char *)&characteristics)[3],peFileHandle)) {
                //if writes before this fail, so will this one
                fclose(peFileHandle);
                return 1;
            }
            foundMatch = 1;
        }

    }
    fclose(peFileHandle);

    if (!foundMatch) {
        return 2; 
    }

    updatePeChecksum(peFile);

    return 0;
}

uint8 setPeSectionCharacteristicsFlags(IN char * peFile, IN char * sectionName,
                                       IN uint32 characteristicsFlags)
{
    uint32 existingCharacteristics = 0;
    uint8 status = 0;

    if (0 != (status = getPeSectionCharacteristics(peFile, sectionName,
            &existingCharacteristics))) {
        return status;
    }

    status = setPeSectionCharacteristics(
            peFile,
            sectionName,
            existingCharacteristics | characteristicsFlags);

    return status;
}

uint8 unsetPeSectionCharacteristicsFlags(IN char * peFile,
                                         IN char * sectionName,
                                         IN uint32 characteristicsFlags)
{
    uint32 existingCharacteristics = 0;
    uint8 status = 0;

    if (0 != (status = getPeSectionCharacteristics(peFile, sectionName,
            &existingCharacteristics))) {
        return status;
    }

    status = setPeSectionCharacteristics(
            peFile,
            sectionName,
            existingCharacteristics & (~characteristicsFlags));

    return status;
}

uint8 getPeChecksum(IN char * peFile, OUT uint32 * existingChecksum, OUT uint32 * computedChecksum)
{
    HINSTANCE imageHlpDll = NULL;

    if (!peFile) {
        return 1;
    }

    if (!existingChecksum) {
        return 2;
    }

    if (!(imageHlpDll = LoadLibrary("ImageHlp.dll"))) { //make DLL available
        return 3;
    }


    if ((FARPROC)NULL == (FARPROC)(MapFileAndCheckSumA =
            (_DLL_FN_MapFileAndCheckSumA) GetProcAddress(imageHlpDll,
            "MapFileAndCheckSumA"))) { //Fetch procedure address
        FreeLibrary(imageHlpDll);
        return 3;
    }

    if (!MapFileAndCheckSumA(peFile, existingChecksum, computedChecksum)) {
        FreeLibrary(imageHlpDll);
        return 1;
    }

    FreeLibrary(imageHlpDll);
    return 0;
      
}

uint8 updatePeChecksum(IN char * peFile)
{
    DWORD existingChecksum = 0;
    DWORD computedChecksum = 0;
    DWORD peHeaderOffset = 0;
    ULONG i = 0; //index reused within various loops
    FILE * peFileHandle = NULL;
    uint8 status;

    if (!(status = getPeChecksum(peFile, &existingChecksum, &computedChecksum))) {
        return status;
    }

    if (existingChecksum == computedChecksum) {
        return 0;
    }

    peFileHandle = fopen(peFile, "r+b");
    if (!peFileHandle) {
        return 1;
    }
    if (0 != fseek(peFileHandle, kMzHeaderOffsetForPeHeaderOffset, SEEK_SET)) {
        fclose(peFileHandle);
        return 1;
    }
    for (i = 0; i < 4; ++i) {
        ((UCHAR *)(&peHeaderOffset))[i] = (UCHAR) fgetc(peFileHandle);
    }
    if (0 != fseek(peFileHandle, ((long)peHeaderOffset)+kPeHeaderOffsetForCheckSum, SEEK_SET)) {
        fclose(peFileHandle);
        return 1;
    }
    for (i = 0; i < 4; ++i) {
        fputc(((UCHAR *)(&computedChecksum))[i], peFileHandle);
    }
    fclose(peFileHandle);

    if (!(status = getPeChecksum(peFile, &existingChecksum, &computedChecksum))) {
        return status;
    }

    if (existingChecksum != computedChecksum) {
        return 1;
    }

    return 0;
}

