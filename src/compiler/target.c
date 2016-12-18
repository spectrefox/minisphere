#include "cell.h"
#include "target.h"

#include "tool.h"

struct target
{
	unsigned int refcount;
	path_t*      name;
	path_t*      path;
	vector_t*    sources;
	tool_t*      tool;
};

target_t*
target_new(const path_t* name, const path_t* path, tool_t* tool)
{
	target_t* target;

	target = calloc(1, sizeof(target_t));
	target->name = path_dup(name);
	target->path = path_dup(path);
	target->sources = vector_new(sizeof(target_t*));
	target->tool = tool_ref(tool);
	return target_ref(target);
}

target_t*
target_ref(target_t* target)
{
	++target->refcount;
	return target;
}

void
target_free(target_t* target)
{
	iter_t iter;
	target_t* *p;

	if (--target->refcount > 0)
		return;

	iter = vector_enum(target->sources);
	while (p = vector_next(&iter))
		target_free(*p);
	vector_free(target->sources);
	tool_free(target->tool);
	path_free(target->name);
	free(target);
}

const path_t*
target_name(const target_t* target)
{
	return target->name;
}

const path_t*
target_path(const target_t* target)
{
	return target->path;
}

void
target_add_source(target_t* target, target_t* source)
{
	target_ref(source);
	vector_push(target->sources, &source);
}

void
target_build(target_t* target)
{
	iter_t iter;
	target_t* *p;

	// build any dependencies first
	iter = vector_enum(target->sources);
	while (vector_next(&iter))
		target_build(*p);

	// TODO: actually build the target
}
