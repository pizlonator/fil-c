#include "vstack.h"

#include <stdlib.h>
#include <assert.h>

//#define DEBUG
#include "debug.h"

struct __vstack_t
{
	cmph_uint32 pointer;
	cmph_uint32 *values;
	cmph_uint32 capacity;
};

vstack_t *vstack_new(void)
{
	vstack_t *stack = (vstack_t *)malloc(sizeof(vstack_t));
	assert(stack);
	stack->pointer = 0;
	stack->values = NULL;
	stack->capacity = 0;
	return stack;
}

void vstack_destroy(vstack_t *stack)
{
	assert(stack);
	free(stack->values);
	free(stack);
}

void vstack_push(vstack_t *stack, cmph_uint32 val)
{
	assert(stack);
	vstack_reserve(stack, stack->pointer + 1);
	stack->values[stack->pointer] = val;
	++(stack->pointer);
}
void vstack_pop(vstack_t *stack)
{
	assert(stack);
	assert(stack->pointer > 0);
	--(stack->pointer);
}

cmph_uint32 vstack_top(vstack_t *stack)
{
	assert(stack);
	assert(stack->pointer > 0);
	return stack->values[(stack->pointer - 1)];
}
int vstack_empty(vstack_t *stack)
{
	assert(stack);
	return stack->pointer == 0;
}
cmph_uint32 vstack_size(vstack_t *stack)
{
	return stack->pointer;
}
void vstack_reserve(vstack_t *stack, cmph_uint32 size)
{
	assert(stack);
	if (stack->capacity < size)
	{
		cmph_uint32 new_capacity = stack->capacity + 1;
		DEBUGP("Increasing current capacity %u to %u\n", stack->capacity, size);
		while (new_capacity	< size)
		{
			new_capacity *= 2;
		}
		stack->values = (cmph_uint32 *)realloc(stack->values, sizeof(cmph_uint32)*new_capacity);
		assert(stack->values);
		stack->capacity = new_capacity;
		DEBUGP("Increased\n");
	}
}
		
