#include "cell.h"
#include "tool.h"

struct tool
{
	unsigned int refcount;
};

tool_t*
tool_new(void)
{
	tool_t* tool;

	tool = calloc(1, sizeof(tool_t));
	return tool_ref(tool);
}

tool_t*
tool_ref(tool_t* tool)
{
	++tool->refcount;
	return tool;
}

void
tool_free(tool_t* tool)
{
	if (--tool->refcount > 0)
		return;
	free(tool);
}
