#ifndef COMTRACE_RELATIONSHIP_GRAPH_H
#define COMTRACE_RELATIONSHIP_GRAPH_H

#include <stddef.h>
#include "com_identity.h"
#include "process_lifecycle.h"

typedef enum {
    COMTRACE_GRAPH_NODE_PROCESS = 1,
    COMTRACE_GRAPH_NODE_THREAD,
    COMTRACE_GRAPH_NODE_ACTIVITY,
    COMTRACE_GRAPH_NODE_CALL,
    COMTRACE_GRAPH_NODE_COM_OBJECT,
    COMTRACE_GRAPH_NODE_CLSID,
    COMTRACE_GRAPH_NODE_IID,
    COMTRACE_GRAPH_NODE_IPID
} COMTRACE_GRAPH_NODE_TYPE;

typedef enum {
    COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD = 1,
    COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS,
    COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL,
    COMTRACE_GRAPH_EDGE_CALL_TARGETS_THREAD,
    COMTRACE_GRAPH_EDGE_THREAD_HAS_ACTIVITY,
    COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO,
    COMTRACE_GRAPH_EDGE_CALL_HAS_ACTIVITY,
    COMTRACE_GRAPH_EDGE_PROCESS_OWNS_OBJECT,
    COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT,
    COMTRACE_GRAPH_EDGE_OBJECT_HAS_CLSID,
    COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID,
    COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID
} COMTRACE_GRAPH_EDGE_TYPE;

typedef enum {
    COMTRACE_GRAPH_SUCCESS = 0,
    COMTRACE_GRAPH_INVALID_ARGUMENT,
    COMTRACE_GRAPH_ALLOCATION_FAILED
} COMTRACE_GRAPH_STATUS;

typedef struct {
    COMTRACE_GRAPH_NODE_TYPE type;
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG process_generation;
    BOOL has_process_generation;
    GUID guid;
    ULONGLONG value;
    ULONGLONG observation_count;
    LONGLONG first_seen_timestamp;
    LONGLONG last_seen_timestamp;
    BOOL has_observed_timestamp;
} COMTRACE_GRAPH_NODE;

typedef struct {
    COMTRACE_GRAPH_EDGE_TYPE type;
    size_t source_index;
    size_t target_index;
} COMTRACE_GRAPH_EDGE;

typedef struct {
    COMTRACE_GRAPH_NODE *nodes;
    size_t node_count;
    size_t node_capacity;
    COMTRACE_GRAPH_EDGE *edges;
    size_t edge_count;
    size_t edge_capacity;
} COMTRACE_RELATIONSHIP_GRAPH;

void comtrace_relationship_graph_init(COMTRACE_RELATIONSHIP_GRAPH *graph);
void comtrace_relationship_graph_destroy(COMTRACE_RELATIONSHIP_GRAPH *graph);

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_add_event(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_COM_OBJECT_IDENTITY *identity,
    const COMTRACE_ACTIVITY_IDS *activity_ids
);

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_add_event_with_lifecycle(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_COM_OBJECT_IDENTITY *identity,
    const COMTRACE_ACTIVITY_IDS *activity_ids,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle
);

size_t comtrace_relationship_graph_node_count(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_NODE_TYPE type
);

size_t comtrace_relationship_graph_edge_count(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_EDGE_TYPE type
);

const COMTRACE_GRAPH_NODE *comtrace_relationship_graph_get_node(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t index
);

const COMTRACE_GRAPH_EDGE *comtrace_relationship_graph_get_edge(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t index
);

#endif
