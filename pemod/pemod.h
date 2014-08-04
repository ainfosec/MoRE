/**
    @file
    CLI app and library to modify PE sections (header file)

    @date 12/13/2011
************************************************************************/

#ifndef _MORE_PEMOD_H_
#define _MORE_PEMOD_H_

#include "../stdint.h"

/*************************************/
// FUNCTION PROTOTYPES
/*************************************/

/**
    Provides contents of characteristics DWORD for first encountered section named sectionName in peFile

    @param peFile String containing PE filename to process
    @param sectionName String containing PE section header name to process
    @param characteristics Pointer to 32 bit uint that recieves section characteristics bytes
    @return uint8 representing parameter number which caused error. 0 = success, 1 = error opening peFile, 2 = error finding sectionName, 3 = error with supplied address for characterstics uint32 pointer
    @note sectionName and peFile must be null terminated. Only first 8 characters (excluding null terminator) are considered from sectionName. sectionName will be copied into an array containing 9 bytes of 0x00 until either 8 characters have been copied or a null terminator has been encountered in sectionName - whichever comes first.
*/
uint8 getPeSectionCharacteristics(IN char * peFile, IN char * sectionName, OUT uint32 *characteristics);

/**
    Sets all encountered section headers named sectionName in PE file peFile, to the value contained in uint32 characteristics parameter

    @param peFile String containing PE filename to process
    @param sectionName String containing PE section header name to process
    @param characteristics 32 bit uint used to set the characteristics for each encountered section in peFile bearing the name sectionName
    @return uint8 representing parameter number which caused error. 0 = success, 1 = error opening peFile, 2 = error finding sectionName
    @note sectionName and peFile must be null terminated. Only first 8 characters (excluding null terminator) are considered from sectionName. sectionName will be copied into an array containing 9 bytes of 0x00 until either 8 characters have been copied or a null terminator has been encountered in sectionName - whichever comes first.
*/
uint8 setPeSectionCharacteristics(IN char * peFile, IN char * sectionName, IN uint32 characteristics);

/**
    Sets all flags contained in characteristics parameter in all encountered section headers named sectionName in PE file peFile

    @param peFile String containing PE filename to process
    @param sectionName String containing PE section header name to process
    @param characteristics 32 bit uint containing the characteristics flags to ensure are enabled for each encountered section in peFile bearing the name sectionName
    @return uint8 representing parameter number which caused error. 0 = success, 1 = error opening peFile, 2 = error finding sectionName
    @note sectionName and peFile must be null terminated. Only first 8 characters (excluding null terminator) are considered from sectionName. sectionName will be copied into an array containing 9 bytes of 0x00 until either 8 characters have been copied or a null terminator has been encountered in sectionName - whichever comes first.
    @note Characteristic flags are set through a bitwise-OR
*/
uint8 setPeSectionCharacteristicsFlags(IN char * peFile, IN char * sectionName, IN uint32 characteristicsFlags);


/**
    Unsets all flags contained in characteristics parameter in all encountered section headers named sectionName in PE file peFile

    @param peFile String containing PE filename to process
    @param sectionName String containing PE section header name to process
    @param characteristics 32 bit uint containing the characteristics flags to ensure are disabled for each encountered section in peFile bearing the name sectionName
    @return uint8 representing parameter number which caused error. 0 = success, 1 = error opening peFile, 2 = error finding sectionName
    @note sectionName and peFile must be null terminated. Only first 8 characters (excluding null terminator) are considered from sectionName. sectionName will be copied into an array containing 9 bytes of 0x00 until either 8 characters have been copied or a null terminator has been encountered in sectionName - whichever comes first.
    @note Characteristic flags are unset through a bitwise-AND masking
*/
uint8 unsetPeSectionCharacteristicsFlags(IN char * peFile, IN char * sectionName, IN uint32 characteristicsFlags);


/**
    Extracts existing checksum from PE file

    @param peFile String containing PE filename to process
    @param existingChecksum pointer to 32 bit uint to recieve existing checksum for peFile
    @param computedChecksum pointer to the 32 bit uint to recieve the freshly computed checksum for peFile
    @return uint8 representing parameter number which caused error. 0 = success, 1 = error opening peFile, 2 = error with suppplied address for existingChecksum pointer, 3 = error with supplied address for computedChecksum pointer, 4 = error using ImageHlp.dll
*/
uint8 getPeChecksum(IN char * peFile, OUT uint32 * existingChecksum, OUT uint32 * computedChecksum);

/**
    Computes checksum for PE file and writes it the PE header of that file

    @param peFile String containing PE filename to process
    @return uint8 representing parameter number which caused error. 0 = success, 1 = error opening peFile, 2 = error using ImageHlp.dll
*/
uint8 updatePeChecksum(IN char * peFile);

#endif // _MORE_PEMOD_H_
