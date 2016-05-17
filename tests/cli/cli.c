/**
    CLI front-end to the MoRE test suite
    @file

    @authors Assured Information Security, Inc.
************************************************************************/

/** This specifies use of CLI and is required by test.c */
#define TESTGUI 0 // MUST COME BEFORE TEST.C INCLUSION!!!
#include "..\test.c" //provides back-end for all tests


/*************************************/
// FUNCTION DEFINITIONS
/*************************************/

/** Prints usage statement */
void printUsage()
{
    printf("This program requires 3 case-insensitive arguments, in no specific order:\n");
    printf("  Alignment type: mixed or isolated\n");
    printf("    mixed: data and instruction on same page where the 4k storage begins\n");
    printf("    isolated: storage page is aligned to a 4k address so that it holds just data\n");
    printf("  Application type: static or dynamic\n");
    printf("    static: application will store data in the program stack and heap\n");
    printf("    dynamic: application will store data in the program .text segment and stack\n");
    printf("  Test number: 1, 2, 3, or 4\n");
    printf("    1 - Calculate pi\n");
    printf("    2 - Perform a wasteful sort\n");
    printf("    3 - Randomly increment array elements\n");
    printf("    4 - Cycle count\n");
    printf("\n");
    printf("Optionally, for tests which support randomization, a seed can be specified.\n");
    printf("  - The default seed is 54321.\n");
    printf("  - Tests 2 and 3 support seed specification.\n");
    printf("  Use --seed or -s followed by a seed value (signed 4 byte integer)\n");
    printf("\n");
}


/**
    Program entry point.

    @param argc integer containing number of entries in argv[]
    @param argv array of character arrays (c-strings) containing arguments
    @return int containing status value, 0 is success, nonzero is failure.
*/
int __cdecl main(int argc, char ** argv)  {
    char *inlineStorage = NULL;
    char *heapStorage = NULL;
    char *storage = NULL;
    unsigned char test = 0;
    unsigned long align = 0;
    unsigned long seedCandidate;
    unsigned long seed;
    unsigned __int64 cycleStart = 0;
    unsigned __int64 cycleEnd = 0;


    if ((argc != 4) && (argc != 6))
    {
        printf("ERROR: Incorrect number of arguments.\n");
        printUsage();
        return 1;
    }

    moreAllocate(&inlineStorage, &heapStorage);

    seedCandidate = seed = (unsigned long)54321;


    while (--argc)
    {
        if ((0 == _stricmp((argv[argc]),"1")) && !test)
        {
            test = 1;
        }
        else if ((0 == _stricmp((argv[argc]),"2")) && !test)
        {
            test = 2;
        }
        else if ((0 == _stricmp((argv[argc]),"3")) && !test)
        {
            test = 3;
        }
        else if ((0 == _stricmp((argv[argc]),"4")) && !test)
        {
            test = 4;
        }
        else if ((0 == _stricmp((argv[argc]),"static")) && !storage)
        {
            storage = heapStorage;
        }
        else if ((0 == _stricmp((argv[argc]),"dynamic")) && !storage)
        {
            storage = inlineStorage;
        }
        else if ((0 == _stricmp((argv[argc]),"isolated")) && !align)
        {
            align = 4096;
        }
        else if ((0 == _stricmp((argv[argc]),"mixed")) && !align)
        {
            align = 1;
        }
        else if ((0 == _stricmp((argv[argc]), "--seed")) && !align)
        {
            seed = seedCandidate;
        }
        else if ((0 == _stricmp((argv[argc]), "-s")) && !align)
        {
            seed = seedCandidate;
        }
        else {
            seedCandidate = (unsigned long)atol(argv[argc]);
        }

    }

    if (!align)
    {
        printf("ERROR: You must specify an alignment preference!\n");
        printUsage();
        free(heapStorage);
        return 1;
    }
    else if (!storage)
    {
        printf("ERROR: You must specify an application preference!\n");
        printUsage();
        free(heapStorage);
        return 1;
    }
    else if (!test)
    {
        printf("ERROR: You must specify a test number!\n");
        printUsage();
        free(heapStorage);
        return 1;
    }

    while (((unsigned long)(++storage)) % align) {}

    memset(storage, 0, 4096);

    printf("Storage is at: %X\n", storage);

    if (1 == align)
    {
        if (0 == (((unsigned long)(--storage))%4096))
        {
            printf("Uh oh. The storage ended up being paged aligned (isolated). This error is an\n");
            printf("issue with the program itself and will need source modification to fix.\n\n");
            printf("Aborting.\n");
            free(heapStorage);
            return 1;
        }
    }

    cycleStart = getTSC();

    switch (test)
    {
        case 1:
            moreTestCalcPi(NULL, (long double *)storage, 900000000);
            break;

        case 2:
            moreTestWastefulSort(NULL, (unsigned long *)storage, 9, seed);
            break;

        case 3:
            moreTestIncrementArray(NULL, (unsigned long *)storage, 1024, 100000, seed);
            break;

        case 4:
            moreTestStopwatch(NULL, (unsigned long long *)storage, 3, 5);
            break;

        default:
            printf("Invalid test number specified. Try again.\n");
            free(heapStorage);
            return 0;
    }

    cycleEnd = getTSC();

    printf("%I64d cycles elapsed\n", cycleEnd-cycleStart);

    free(heapStorage);
    return 0;
}
