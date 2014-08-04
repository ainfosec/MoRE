/**
	@file
	Code to provide a FIFO stack data structure
		
	@date 1/25/2012
***************************************************************/

#include "ntddk.h"
#include "stack.h"

void StackInitStack(Stack * stack)
{
    stack->empty = 1;
    stack->top = 0;
}

void * StackPop(Stack * stack)
{
    void *outVal;
    if (stack->empty == 1)
        return NULL;
        
    outVal = stack->data[stack->top];
    
    if (stack->top == 0)
    {
        stack->empty = 1;
    }
    else
    {
        stack->top--;
    }
    
    return outVal;
}

void StackPush(Stack * stack, void * ptr)
{
    if (stack->top + 1 == MAX_STACK_SIZE)
        return;
        
    if (stack->empty == 1)
    {
        stack->top = 0;
        stack->empty = 0;
        stack->data[0] = ptr;
    }
    else
    {
        stack->top++;
        stack->data[stack->top] = ptr;
    }
}

uint8 StackIsEmpty(Stack * stack)
{
    return stack->empty;
}

uint8 StackIsFull(Stack * stack)
{
    return (stack->top + 1 == MAX_STACK_SIZE) ? 1 : 0;
}

uint32 StackNumEntries(Stack * stack)
{
    if (stack->empty == 1)
        return 0;
    return stack->top + 1;
}

void * StackPeek(Stack * stack)
{
    if (stack->empty == 1)
        return NULL;
    return stack->data[stack->top];
}
