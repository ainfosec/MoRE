/**
	@file
	Header file to provide a FIFO stack data structure
		
	@date 1/25/2012
***************************************************************/

#include "stdint.h"

/** Maximum number of elements in the stack at a time */
#define MAX_STACK_SIZE 100

/** Structure to hold the needed information for a stack */
struct Stack_s
{
    void *data[MAX_STACK_SIZE];
    uint32 top;
    uint8 empty;
};

typedef struct Stack_s Stack;

/**
    Intializes (or reinitializes) a stack
    
    @param stack Pointer to the stack
*/
void StackInitStack(Stack * stack);

/**
    Pops off the top values on the stack
    
    @param stack Pointer to the stack
    @return Top value of the stack or NULL if the stack is empty
*/
void * StackPop(Stack * stack);

/**
    Returns the top value without popping it off
    
    @param stack Pointer to stack
    @return Top element in stack
*/
void * StackPeek(Stack * stack);

/**
    Pushes an element onto the stack
    
    @param stack Pointer to the stack
    @param ptr Pointer to push
    
    @note If the stack is full, the item is not pushed
*/
void StackPush(Stack * stack, void * ptr);

/**
    Boolean function to determine if the stack is full
    
    @param stack Pointer to stack
    @return 1 if full, 0 otherwise
*/
uint8 StackIsFull(Stack * stack);

/**
    Boolean function to determine if the stack is empty
    
    @param stack Pointer to stack
    @return 1 if empty, 0 otherwise
*/
uint8 StackIsEmpty(Stack * stack);

/**
    Returns the number of entries in the stack
    
    @param stack Pointer to the stack
    @return Number of entries in the stack
*/
uint32 StackNumEntries(Stack * stack);
