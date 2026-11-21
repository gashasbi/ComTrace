#ifndef COMTRACE_OBJECT_IDENTITY_COMPARE_H
#define COMTRACE_OBJECT_IDENTITY_COMPARE_H

#include <stddef.h>
#include <windows.h>
#include "relationship_graph.h"

#define COMTRACE_OBJECT_CHANGE_CLSID 0x00000001UL
#define COMTRACE_OBJECT_CHANGE_IID 0x00000002UL
#define COMTRACE_OBJECT_CHANGE_IPID 0x00000004UL

ULONG comtrace_compare_object_identity_fields(
    const COMTRACE_RELATIONSHIP_GRAPH *left_graph,
    size_t left_index,
    const COMTRACE_RELATIONSHIP_GRAPH *right_graph,
    size_t right_index
);

#endif
