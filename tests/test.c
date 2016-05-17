#ifndef _MoRE_test_c
#define _MoRE_test_c
/**
    Provides test implementations to MoRE test suite front ends.
    Include this file, with a TESTGUI value #defined, to use tests
    @file

    @authors Assured Information Security, Inc.
************************************************************************/

/* How to interface with this file from a new UI:
 *
 * Step 1: TESTGUI
 * - Look below, see what the highest TESTGUI value defined is, and add 1 to it.
 * - Now, make a #if case for that TESTGUI value for the definition of MoreTopWindow
 * - Anywhere you need to perform instructions specific to your UI that would
 *   conflict with another UI, #if TESTGUI == yourvalue and #endif afterward
 * - Include test.c from your UI's source file and #define TESTGUI yourvalue
 *   before the include statement. Yes, you need to include a .c file and it
 *   must be compiled as part of your UI app in order to work properly! I 
 *   recognize this is less than ideal, but it was the most viable way to allow
 *   a developer to select a GUI style and have certain code compiled/omitted
 *   as a result - on the upside, it provides nearly full autonomy over how you
 *   wish to have test.c and your UI interact, you simply need to determine what
 *   to do with messages being output on different numbered channels (known as
 *   locations), which is explained below. What you do with that information
 *   and how you do it is completely up to you.
 * - For now, just do the #if inside of the struct/class/prototype definition
 *   section of this file.
 *
 *
 * Step 2: Define MoreTopWindow
 * - MoreTopWindow is a class/struct/variable - whichever, common to both this
 *   file and the UI source code, should at least be prototyped in the code here
 *   so that it can be interacted with by other relevant functions
 * - MoreTopWindow must be able to in some way be able to have any messages to
 *   display be communicated to it. So, it must contain a destination for these
 *   messages, hence the pointer naming convention of "destination"
 * - Examples:
 *       CLI: MoreTopWindow is typedefed to void * because it doesn't matter
 *       Ultimate++: MoreTopWindow is an actual class and all GUI windows used
 *   for the tests in Ultimate ++ inherit from it. It has a display method
 *   which allows messages and locations to be funneled to it and reroutes said
 *   messages to the appropriate label on the GUI window
 *       Windows GDI: MoreTopWindow is a struct containing various bits of
 *   information required to display each message from test.c, including the
 *   handle to the main window, the number of the update procedure to call, and
 *   a table of strings to load into the UI. Every time the update procedure is
 *   called, the UI is updated with those strings.
 *   
 *   Essentially, MoreTopWindow is to be thought of as "whatever" is needed to
 *   give the display function in test.c (which you need to modify) enough
 *   information to display a string on the UI
 *
 *
 * Step 3: Display function
 * - The display function needs to be modified with a #if TESTGUI == yourvalue
 *   section that contains instructions to render messages fed to the display
 *   function onto the UI you've created. The display function is supplied with
 *   a "location" value, that indentifies the element to map the message to on
 *   your UI. The number and nature of locations is identified in the comments
 *   prior to each test function. For example, a test may have a status output
 *   a progress output, and a value output, number 0, 1, 2, respectively. As
 *   such, output 0 might start as "Processing...", output 1 as "0", and output
 *   2 as "0". As the test goes, output 2 may grow or change as some calculation
 *   is being performed, and output 1 will slowly approach 100, which would
 *   signal processing has been completed at that point, and output 0 would
 *   then change to "Done!". In your UI, you could, if you desired, set it up
 *   so that when your UI was notified by the display function about new data
 *   for output 0, it updated the title bar with that text, and you could have
 *   the UI convert the text of the process (output 1) to an integer and use
 *   that value to drive a progress meter widget, and have a label on the
 *   window show the value of output 2 as it fluctuates. On the UI side you
 *   can add additional widgets as you desired, and, if it eases the burden
 *   of development, you could assign them additional inputs and call display
 *   from within the GUI to update them, for so long as you do not modify the
 *   outputs used within the tests, which would break backwards compatibility.
 *
 * - Examples:
 *       CLI: Destination is just null, location is completely ignored
 *   Everything that is sent to the display function is printf'd by test.c. 
 *   This admittedly shrugs the responsibility off for printing things inside
 *   the UI file, but also simplies future CLI implementations substantially
 *       Ultimate++: Destination is the GUI window to render to, and display
 *   calls Destination->Display to render the data in the UI code. The Display
 *   method is overriden in each window class for each test and contains a
 *   switch statement for handling the location value and ensuring the
 *   appropriate UI element is updated.
 *       Windows GDI: Destination struct has the data members needed to
 *   issue a message to destroy and recreate the elements on the target
 *   window and the processing of that message (in the UI code) while creating
 *   the elements of the UI that were destroyed, will populate their values
 *   based upon a table of strings that is also in the destination struct. The
 *   display function updates this string table just prior to issuing the
 *   message to destroy/create the elements on the GUI
 *
 *
 * Step 4: processUIEvents function
 * - Simply do whatever needs doing in here that causes pending events in your
 *   UI to be processed. If you are running a multithreaded UI or have some
 *   other magic behind the scenes, this may not be necessary. Like the other
 *   functions you've manipulated, it needs to use #if TESTGUI == yourvalue
 *
 * - Examples:
 *       CLI: Doesn't need to do anything.
 *       Ultimate++: Calls methods on the destination class, which is the
 *   window that needs to be updated, to perform such updating and to block
 *   until all events for the window has been processed.
 *       Windows GDI: Manually goes into an event processing loop and
 *   continues until no further events need processing, then relenquishes
 *   control back to the caller.
 *
 *
 * Step 5: checkHalt function
 * - A means must be accessible through MoreTopWindow that is set if a stop
 *   or close action is requested by the user. This function must check to see
 *   if a stop has been requested and return a nonzero value if so, zero
 *   otherwise.
 * - This doesn't need to be a variable, could be an event, or anything that
 *   you have the means of checking on.
 * - CLI doesn't need this, due to the ability to just press control-c and
 *   break the execution like any other CLI app.
 * - All GUI implementations so far have simply made this a variable and set
 *   event handlers for any cancelation buttons (such as the X in the window
 *   title bar) to set the halt option as well as process the close event if
 *   desired (such as if no test is running, or the window needs to close after
 *   the cancelation has occured). As such, the checkHalt function just returns
 *   the value of that variable for these UIs.
 * - As always, use #if TESTGUI == yourvalue so as not to break other UIs
 *
 *
 * Step 6: moreAllocate and getTSC
 * - moreAllocate must be called by your UI to obtain the storage to be used
 *   for the test. moreAllocate will obtain an 8k blob of memory from both the
 *   .text section of the instructions in the code and from the heap via malloc
 * - Your UI will need to determine (by user choice or whatever you desire)
 *   which blob of memory to use and it will be passed in to each test as
 *   indicated in the comments above each test function. The tests will,
 *   at most, use 4k of this space, if used as instructed in the comments.
 * - Any sort of alignment that you may have seen in GUI/CLI apps are being
 *   done by the app itself, and is optional.
 * - getTSC will fetch the current value from the CPU time stamp counter. By
 *   doing this directly before and after calling the test you desire, and
 *   subtracting the prior value from the latter, you will obtain the number of
 *   CPU cycles elapsed during the test, useful for performance metrics
 *
 *
 * Step 7: Implementing other functions
 * - You may choose to implement other functions as you see fit, but if they
 *   are not designed to service every UI that interacts with test.c, please
 *   utilize #if TESTGUI == yourvalue to selectively compile the function
 *   declaration and implementation.
 * - This should only be done for functions that might provide useful
 *   functionality to other UIs down the road, or to functions that need to be
 *   called within test.c - otherwise, please define these in your UI's source
 * - Example:
 *       Windows GDI: resetStringTable - this is used to prepare the string
 *   table for a GDI style update and could be useful for similar UIs.
 *   Importantly, it does not interact with anything GDI specific, therefore a 
 *   MoreTopWindow be it a struct or class that had data members stringTable
 *   and stringTableNumEntries could make use of this function.
 */

/* How to define a new test:
 * - Define a function in the pattern of moreTestTestName replacing TestName
 *   with the name of your test
 * - Minimally pass a pointer to a MoreTopWindow for the destination to be used
 *   with the display function, and a pointer to the storage region, 4k of which
 *   is assumed to be available for use by the test.
 * - Don't bother returning anything
 * - Keep messages to be displayed under 256 characters, and remember, some UIs
 *   may not even be able to comfortably display messages that large.
 * - When outputting data, remember, all data will be outputted to the CLI, and
 *   remember that it's okay to rely on the UI implementation to label certain
 *   types of data. Do your best to represent data as just numbers when it
 *   makes sense to and could be fed into another widget as such.
 * - Use display(NULL, 0, "message") to display a message to the CLI but not
 *   to any GUI front-ends.
 * - Within your test function, always zero out the portion of the storage area
 *   that you intend to use, prior to using it.
 * - Use checkHalt in any relevant loops that will need to break if the test is
 *   requested to be aborted
 * - Use checkHalt to skip any non-necessary followups after the test, such as
 *   displaying the results, if the test was aborted
 * - If using entropy at all, be sure to pass the seed as an argument into your
 *   test function and to call srand inside the test function, on the seed
 */

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <varargs.h>
#include <assert.h>


/*************************************/
// MACROS AND COMPILE-TIME SETTINGS
/*************************************/

/* Used to create huge numbers of x86 asm nop instructions, quickly/easily */
#define NOP2 __asm NOP __asm NOP
#define NOP4 NOP2 NOP2
#define NOP8 NOP4 NOP4
#define NOP16 NOP8 NOP8
#define NOP32 NOP16 NOP16
#define NOP64 NOP32 NOP32
#define NOP256 NOP64 NOP64 NOP64 NOP64
#define NOP1K NOP256 NOP256 NOP256 NOP256
#define NOP4K NOP1K NOP1K NOP1K NOP1K

/** msgBuffer size */
#define BUFFERSIZE 256


/*************************************/
// STRUCT/CLASS/TYPEDEF DECLARATIONS
/*************************************/

/** TESTGUI value MUST BE defined! */
#ifndef TESTGUI
#error : TESTGUI value must be defined in source code
#endif

/* MoreTopWindow - what is it?
 *
 * MoreTopWindow is a placeholder name, that originated from the Ultimate++ GUI
 * front end, it is used to mean "the gui" essentially, which is why it is
 * typedefed to a void* for CLI applications, because it is ignored completely.
 * However, when rendering to a GUI, MoreTopWindow reflects what is being
 * rendered into. This will become more clear in the display function comments.
 */

/** CLI front-end */
#if TESTGUI == 0
/** No GUI = no MoreTopWindow, thus, typeless */
typedef void * MoreTopWindow;

/** Ultimate++ GUI front-end */
#elif TESTGUI == 1 //IF THIS IS TRUE, THIS FILE IS NOW C++ NOT C
/** Include is needed to utilize Ultimate++ GUI classes */
#include <CtrlLib/CtrlLib.h>
using namespace Upp;

/** MoreTopWindow will be the base window class for the GUI windows */
class MoreTopWindow : public TopWindow
{
    public:
        /** Required by Ultimate++ to make callbacks work*/
        typedef MoreTopWindow CLASSNAME;

        /** A common label, defined here for convenience */
        Label status;

        /** Every test UI window has a start button */
        Button start;
        
        /**
            MoreTopWindow Constructor

            @param storage pointer to 4k storage region
            @param seed value to seed random number generator with
        */
        MoreTopWindow(char * storage, unsigned long seed);

        /** Executed when close button is pressed in title bar */
        void Close();

        /** Executed when start button is pushed */
        void pushStart();

        /** Override with a function that calls the appropriate test for that GUI */
        virtual void runTest() = 0;

        /** 
            Override with a function that routes incoming strings from back-end to
            the correct labels on the GUI window

            @param location output identifier used to route output to correct UI element
            @param message string containing the actual output
        */
        virtual void display(unsigned long location, const char * message) = 0;

        /** Nonzero if test needs requested to be interrupted */
        unsigned char halt;
        
    protected:
        /** Pointer to the storage region selected */
        char * storage;

        /** Random number generator seed value */
        unsigned long seed;
};

/** Windows GDI front-end */
#elif TESTGUI == 2
/** Contains vital information needed to update the GDI GUI Window */
typedef struct MoreTopWindow
{
    /** HWND Window object being rendered into */
    HWND hWnd;

    /** Was close button pressed mid test? Nonzero if so. */
    unsigned char halt;

    /** How many strings in array of strings, stringTable */
    unsigned long stringTableNumEntries;

    /** Array of strings used to update the UI elements */
    char **stringTable;

    /** What WndProc WM_COMMAND lParam value should be used? */
    UINT updateMsgNum;
} MoreTopWindow;

/** Unsupported TESTGUI value*/
#else 
#error : Invalid TESTGUI value defined
#endif


/*************************************/
// GLOBAL DATA
/*************************************/

/** Reused to tempoarily hold contents of string being assembled to display */
char msgBuffer[BUFFERSIZE];


/*************************************/
// FUNCTION DEFINITIONS
/*************************************/

#if TESTGUI == 2
/**
    Cleans up and prepares the string table in MoreTopWindow for new use

    @note This is only relevant to Windows GDI apps
    
    @param destination pointer to current MoreTopWindow data structure
    @param newNumEntries number of entries to allocate for destination->stringTable
*/
void resetStringTable(MoreTopWindow * destination, unsigned long newNumEntries)
{
    while (destination->stringTableNumEntries && destination->stringTable)
    {
        free(destination->stringTable[--(destination->stringTableNumEntries)]);
    }

    if (destination->stringTable)
    {
        free(destination->stringTable);
        destination->stringTable = NULL;
    }
    
    if (destination->stringTableNumEntries = newNumEntries)
    {
        destination->stringTable =
            (char **) malloc(sizeof(char *) * (destination->stringTableNumEntries));
        memset(destination->stringTable, 0,
               sizeof(char *) * (destination->stringTableNumEntries));
    }
}
#endif

/**
    Determines if halt is requested

    @note returns 0 if invoked by a CLI app. Control-c aborts just fine!

    @param destination pointer to current MoreTopWindow
    @return nonzero if halt requested
*/
unsigned char checkHalt(MoreTopWindow * destination)
{
/* These weren't always identical */
#if TESTGUI == 1
    if (destination)
    {
        return destination->halt;
    }
#endif
#if TESTGUI == 2
    if (destination)
    {
        return destination->halt;
    }
#endif
    return 0;
}

/**
    Processes any pending GUI events

    @note does nothing for a CLI app

    @param destination pointer to current MoreTopWindow
*/
void processUIEvents(MoreTopWindow * destination)
{
#if TESTGUI == 1
    if (destination)
    {
        destination->ProcessEvents();
    }
#endif
#if TESTGUI == 2
    if (destination)
    {
        MSG Msg;
        /* MSDN suggests values are never negative for PeekMessage, unlike GetMessage */
        while ((!(destination->halt)) &&
               (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD) > 0))
        {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
        if (WM_QUIT == Msg.message)
        {
            destination->halt = 1;
        }

    }
#endif
}

/**
    Displays a given string onto the UI

    @note This is the focal point for back-end and UI interaction
    @note UI elements are numbered from 0, sequentially

    @param destination pointer to current MoreTopWindow, always NULL from CLI
    @param location unsigned long specifying what UI element to update
    @param message printf style formatting string and parameter list
*/
void display(MoreTopWindow * destination, unsigned long location,
             const char * message, ...)
{
    va_list args;
    
    msgBuffer[0] = '\0';
    va_start(args, message);
    _vsnprintf_s(msgBuffer, BUFFERSIZE, _TRUNCATE, message, args);
    va_end(args);

#if TESTGUI == 1
    if (destination)
    {
        /* Displaying is handled by the display method of the target window */
        destination->display(location, msgBuffer);
        /* Repaints */
        destination->Refresh();
        /* Runs the Ultimate++ event processing loop to process UI changes/repaint */
        processUIEvents(destination);
    }
#endif
#if TESTGUI == 2
    if (destination)
    {
        if (destination->stringTable &&
            (location < (destination->stringTableNumEntries)))
        {
            /* Frees if necessary, then sets location-indexed string to message */
            if (destination->stringTable[location])
            {
                free(destination->stringTable[location]);
            }
            destination->stringTable[location] = (char *)malloc(strlen(msgBuffer)+1);
            strcpy_s(destination->stringTable[location], strlen(msgBuffer)+1, msgBuffer);
        }
        /* updateMsgNum is equal to the case to call in WndProc to delete the
         * currently rendered UI elements and update them appropriately, and
         * of course, said WndProc switch case utilizes the stringTable to
         * obtain the text to render into each label on the UI */
        SendMessage(destination->hWnd, WM_COMMAND, destination->updateMsgNum, 0);
        /* Runs the Windows GDI event processing loop (manually...) */
        processUIEvents(destination);
    }
#endif
#if TESTGUI == 0
    /* And a simple printf for CLI applications. Location/destination ignored! */
    printf(msgBuffer);
#endif
}


/**
    Test 1: Approximates pi slowly and sloppily so as to waste cycles

    @note 3 outputs
    @note Output 0: Status
    @note Output 1: Result (as it is being computed, as well as the final)
    @note Output 2: Number of iterations (just the number, no label)

    @param destination pointer to current MoreTopWindow
    @param result set this to the storage location cast to (long double *)
    @param iterations how long should the approximation continue (900000000 is convention)
*/
void moreTestCalcPi(MoreTopWindow * destination, long double *result,
                    unsigned long long iterations)
{
    unsigned long long i = 0; //used to track iterations

    /* Test prep */
    memset(result, 0, sizeof(long double));
    (*result) = 0.0;

    /* Test loop */
    for (i = 0; i <= iterations && !checkHalt(destination); ++i)
    {
        if (0 == (i % 10000000)) //adjust this value to update UI more frequently
        {
            display(destination, 0, "Calculating ");
            display(destination, 1, "Pi: %1.30llf", (*result)*4);
            display(NULL, 0, ", at ");
            display(destination, 2, "%llu", i);
            display(NULL, 0, " iterations\n", i);
        }
        /* Pi approximation: Sum from i->infinity of ((-1)^i)/(1+(i*2)) */
        if (0 == (i%2)) //intentionally sloppy to slow up calculation
        {
            (*result) += ((float)1)/((float)(1+(i*2))); //casts keep the ints away
        }
        else
        {
            (*result) -= ((float)1)/((float)(1+(i*2)));
        }
    }
    
    /* Results */
    if (!checkHalt(destination)) //Skip the results if test aborted
    {
        (*result) *= 4;
        display(destination, 0, "Done");
        display(NULL, 0, "\n");
        display(destination, 1, "Pi: %1.30llf", (*result));
        display(NULL, 0, "\n");    
    }
}

/**
    Convenience function to populate array with random numbers

    @param array array of unsigned longs to fill with random numbers
    @param count number of elements in array
*/
void populateRandomly(unsigned long * array, unsigned long long count)
{
    while (count--)
    {
        array[count] = rand();
    }
}

/**
    Test 2: Performs a random sort continually, stopping when array is sorted

    @note 5 outputs
    @note Output 0: Status (will say "Sorting" while sorting)
    @note Output 1: "."  Outputs 1-3 are used to show a trail of periods that
    @note Output 2: "." will appear and disappear as the sort occurs. These are
    @note Output 3: "." intended to show up afer "Sorting" e.g. "Sorting ..."
    @note Output 4: Result

    @note This function may never return under certain cases
    @note Non-return case 1: Count is too high, too many elements to sort quickly
    @note Non-return case 2: Seed produces non-sorted state that persists too long

    @param destination pointer to current MoreTopWindow
    @param array set this to the storage region cast to (unsigned long *)
    @param count how many items to populate the array with and sort (9 is convention)
    @param seed random number generator seed (54321 is convention)
*/
void moreTestWastefulSort(MoreTopWindow *destination, unsigned long * array,
                          unsigned long count, unsigned int seed)
{
    unsigned char unsorted = 1; //1 if array currently unsorted
    unsigned long long position = 0; //array index during sort and check
    unsigned long tmp = 0; //holds value for swap during sort
    unsigned long long itr = 0; //tracks iterations for UI update and results
    unsigned char sorting = 1; //1-3 = number of dots to show for status, 4 = no dots

    /* Test prep */
    memset(array, 0, count*sizeof(unsigned long));
    srand(seed);
    display(destination, 0, "Populating an array with random numbers...");
    populateRandomly(array, count);
    display(NULL, 0, "   Done!\n");

    /* Test loop */
    display(destination, 0, "Sorting");
    display(NULL, 0, "...");
    while (unsorted && !checkHalt(destination)) //loop til sorted, start by sorting regardless
    {
        /* for each element in the array, randommly swap, or don't */
        for (position = 0; position < (count-1); ++position)
        {
            if (rand()%2)
            {
                tmp = array[position];
                array[position] = array[position+1];
                array[position+1] = tmp;
            }
        }
        unsorted = 0;
        ++itr; //1 more sort done

        if (0 == itr % 10000000) //UI update frequency
        {
            /* the following makes the ... animate on the screen as
             * .
             * ..
             * ...
             * (no dots)
             * (repeat above)
             */
            if (sorting < 4) //only ranges from 1-4 due to manipulation in else
            {
                display(destination, sorting++, "."); //Sorting... (this shows the dots one by one)
            }
            else //this hides the dots
            {
                display(destination, 1, "");
                display(destination, 2, "");
                display(destination, 3, "");
                sorting = 1;
            }
        }

        /* determine if array is sorted in descending order */
        for (position = 0; position < (count-1); ++position)
        {
            if (array[position] > array[position+1])
            {
                unsorted = 1;
            }
        }
    }

    /* Test results */
    if (!checkHalt(destination))
    {
        display(NULL, 0, "   ");
        display(destination, 0, "Done!");
        display(NULL, 0, "\n");
        display(destination, 4, "Sorted in %llu iterations.", itr);
        display(NULL, 0, "\n");
    }
}

/**
    Test 3: Randomly increments each element of an array

    @note 1 output
    @note Output 0: Status and result

    @param destination pointer to current MoreTopWindow
    @param array set this to the storage region cast to (unsigned long *)
    @param count how many items to populate the array with and randomly increment (1024 is convention)
    @param cycles how many iterations of the random increment loop (100000 is convention)
    @param seed random number generator seed (54321 is convention)
*/
void moreTestIncrementArray(MoreTopWindow *destination, unsigned long *array,
                            unsigned long count, unsigned long cycles,
                            unsigned long seed)
{
    unsigned long position = 0;
    unsigned long long total = 0;
    unsigned long long lasttotal = 0;
    unsigned long cyclestmp = 0;

    /* Test Prep */
    memset(array, 0, count*sizeof(unsigned long));
    srand(seed);
    cyclestmp = cycles;

    /* Test Loop */
    display(destination, 0, "Performing random increments...");
    while (cyclestmp-- && !checkHalt(destination))
    {
        for (position = 0; position < count; ++position) //for each array element
        {
            if (rand()%2) //either increment by 1, or don't 50-50 chance
            {
                ++(array[position]);
            }
        }
    }

    /* Totals up the array */
    if (!checkHalt(destination))
    {
        total = lasttotal = 0;
        for (position = 0; position < count; ++position)
        {
            total += array[position];
            if (total < lasttotal)
            {
                display(destination, 0,
                        "Total grew to be too large and wrapped around. Aborting.");
                return;
            }
            lasttotal = total;
        }
    }

    /* Test Results */
    if (!checkHalt(destination))
    {
        display(NULL, 0, "\n");
        display(destination, 0, "Random Increments completed. Average: %lf",
                (((double)total)/((double)count))/(double)cycles); //About .5
        display(NULL, 0, "\n");
    }
}

/**
    Test 4: Runs timers and measures how many cycles can elapse while a timer runs

    @note 2 outputs
    @note Output 0: Status
    @note Output 1: Progress and result

    @param destination pointer to current MoreTopWindow
    @param timers set this to the storage region cast to (unsigned long long *)
    @param seconds how many seconds to run each timer (3 is convention)
    @param count how many timers to run (5 is convention) timers alarm consecutively
*/
void moreTestStopwatch(MoreTopWindow *destination, unsigned long long *timers,
                       unsigned long long seconds, size_t count)
{
    unsigned long long position = 0;
    unsigned long long start = 0;
    unsigned char *alarmed = NULL;
    unsigned long long cycles = 0;

    /* Test Prep */
    alarmed = (unsigned char *) malloc(count);
    memset(alarmed, 0, count);
    memset(timers, 0, count*sizeof(unsigned long long));

    /* Test Loop */
    display(destination, 0, "The timers are running...");
    start = GetTickCount64();
    display(NULL, 0, "\n");    
    while (!(alarmed[count-1]) && !checkHalt(destination)) //loop till all alarmed
    {
        /* Check the timer as often as possible */
        for (position =0; position < count; ++position)
        {
            ++cycles; //How many iterations possible before the timer alarms?
            timers[position] = GetTickCount64();
            /*
             * Timers actually run concurrently, but seem consecutive because
             * for each additional timer requested, another is added to the 
             * array that alarms at seconds (parameter) later than the previous
             */
            /* If the timer indicates the specified time has passed */
            if ( (!(alarmed[position])) && (timers[position]
                >= (start + (seconds*1000*(position+1)))) ) 
            {
                display(destination, 1, "Timer %llu alarmed!", position+1);
                display(NULL, 0, "\n");
                alarmed[position] = 1; //Time's up!
            }
        }
    }

    free(alarmed);
    
    /* Test Results */
    if (!checkHalt(destination))
    {
        display(destination, 0, "Done! ");
        display(destination, 1,
                "Each timer alarmed in %llu iterations on average", cycles/5);
        display(NULL, 0, "\n");
    }
}

/**
    Gets the inline .text section and heap storage offsets

    @notes makes 8k of nops in .text and then returns a pointer to start of them

    @param inlineStorage pass address of a char * in to recieve 8k inline storage address
    @param heapStorage pass address of a char * in to recieve 8k heap storage address
*/
void moreAllocate(char ** inlineStorage, char ** heapStorage)
{
    char * nopAddr;

    __asm
    {
        push eax
        mov eax, startOfNops
        mov nopAddr, eax
        pop eax
        jmp endOfNops
        startOfNops:
    }

    /* 8K of NOPs made by two macros that makes 4096 NOP instructions */
    NOP4K ; 
    NOP4K ; 
    
    __asm
    {
        endOfNops:
    }
    
    *inlineStorage = nopAddr;
    
    *heapStorage = (char*) malloc(4096*2);
}

/**
    @return value from the CPU time stamp counter 
*/
unsigned __int64 __cdecl getTSC()
{
    ULARGE_INTEGER tsc = {0};
    DWORD tscLow = 0;
    DWORD tscHigh = 0;

    __asm
    {
        push eax
        push ebx
        push ecx
        push edx
        /* run CPUID instruction to force serialization */
        __asm __emit 0x0f __asm __emit 0xa2 //emits the cpuid opcode
        __asm __emit 0x0f __asm __emit 0x31 //emits the rdtsc opcode
        mov tscLow, eax
        mov tscHigh, edx
        pop edx
        pop ecx
        pop ebx
        pop eax
    }

    tsc.LowPart = tscLow;
    tsc.HighPart = tscHigh;

    return tsc.QuadPart;
}


#endif // _MoRE_test_c
