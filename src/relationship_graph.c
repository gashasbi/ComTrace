#include "relationship_graph.h"

#include <stdlib.h>
#include <string.h>

static int ensure_node_capacity(COMTRACE_RELATIONSHIP_GRAPH *graph)
{
    size_t capacity;
    COMTRACE_GRAPH_NODE *nodes;

    if (graph->node_count < graph->node_capacity) {
        return 1;
    }

    capacity = graph->node_capacity == 0U
        ? 16U
        : graph->node_capacity * 2U;

    if (capacity < graph->node_capacity ||
        capacity > (size_t)-1 / sizeof(*nodes)) {
        return 0;
    }

    nodes = (COMTRACE_GRAPH_NODE *)realloc(
        graph->nodes,
        capacity * sizeof(*nodes)
    );
    if (nodes == NULL) {
        return 0;
    }

    graph->nodes = nodes;
    graph->node_capacity = capacity;
    return 1;
}

static int ensure_edge_capacity(COMTRACE_RELATIONSHIP_GRAPH *graph)
{
    size_t capacity;
    COMTRACE_GRAPH_EDGE *edges;

    if (graph->edge_count < graph->edge_capacity) {
        return 1;
    }

    capacity = graph->edge_capacity == 0U
        ? 32U
        : graph->edge_capacity * 2U;

    if (capacity < graph->edge_capacity ||
        capacity > (size_t)-1 / sizeof(*edges)) {
        return 0;
    }

    edges = (COMTRACE_GRAPH_EDGE *)realloc(
        graph->edges,
        capacity * sizeof(*edges)
    );
    if (edges == NULL) {
        return 0;
    }

    graph->edges = edges;
    graph->edge_capacity = capacity;
    return 1;
}

static int same_node(
    const COMTRACE_GRAPH_NODE *left,
    COMTRACE_GRAPH_NODE_TYPE type,
    DWORD process_id,
    DWORD thread_id,
    const GUID *guid,
    ULONGLONG value,
    ULONGLONG process_generation,
    BOOL has_process_generation
)
{
    if (left->type != type) {
        return 0;
    }

    switch (type) {
    case COMTRACE_GRAPH_NODE_PROCESS:
        return left->process_id == process_id &&
               left->has_process_generation == has_process_generation &&
               (!has_process_generation ||
                left->process_generation == process_generation);
    case COMTRACE_GRAPH_NODE_THREAD:
        return left->process_id == process_id &&
               left->thread_id == thread_id &&
               left->has_process_generation == has_process_generation &&
               (!has_process_generation ||
                left->process_generation == process_generation);
    case COMTRACE_GRAPH_NODE_ACTIVITY:
    case COMTRACE_GRAPH_NODE_CLSID:
    case COMTRACE_GRAPH_NODE_IID:
    case COMTRACE_GRAPH_NODE_IPID:
        return guid != NULL &&
               memcmp(&left->guid, guid, sizeof(GUID)) == 0;
    case COMTRACE_GRAPH_NODE_CALL:
        return left->value == value;
    case COMTRACE_GRAPH_NODE_COM_OBJECT:
        return left->process_id == process_id &&
               left->value == value &&
               left->has_process_generation == has_process_generation &&
               (!has_process_generation ||
                left->process_generation == process_generation);
    default:
        return 0;
    }
}

static COMTRACE_GRAPH_STATUS add_node_with_generation(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_NODE_TYPE type,
    DWORD process_id,
    DWORD thread_id,
    const GUID *guid,
    ULONGLONG value,
    ULONGLONG process_generation,
    BOOL has_process_generation,
    size_t *index
)
{
    size_t current;
    COMTRACE_GRAPH_NODE *node;

    for (current = 0U; current < graph->node_count; ++current) {
        if (same_node(
                &graph->nodes[current],
                type,
                process_id,
                thread_id,
                guid,
                value,
                process_generation,
                has_process_generation)) {
            *index = current;
            return COMTRACE_GRAPH_SUCCESS;
        }
    }

    if (!ensure_node_capacity(graph)) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }

    current = graph->node_count;
    node = &graph->nodes[current];
    ZeroMemory(node, sizeof(*node));
    node->type = type;
    node->process_id = process_id;
    node->thread_id = thread_id;
    node->process_generation = process_generation;
    node->has_process_generation = has_process_generation;
    node->value = value;

    if (guid != NULL) {
        node->guid = *guid;
    }

    ++graph->node_count;
    *index = current;
    return COMTRACE_GRAPH_SUCCESS;
}

static COMTRACE_GRAPH_STATUS add_node(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_NODE_TYPE type,
    DWORD process_id,
    DWORD thread_id,
    const GUID *guid,
    ULONGLONG value,
    size_t *index
)
{
    return add_node_with_generation(
        graph,
        type,
        process_id,
        thread_id,
        guid,
        value,
        0ULL,
        FALSE,
        index
    );
}

static COMTRACE_GRAPH_STATUS add_edge(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_EDGE_TYPE type,
    size_t source_index,
    size_t target_index
)
{
    size_t current;
    COMTRACE_GRAPH_EDGE *edge;

    if (source_index >= graph->node_count ||
        target_index >= graph->node_count) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }

    for (current = 0U; current < graph->edge_count; ++current) {
        edge = &graph->edges[current];
        if (edge->type == type &&
            edge->source_index == source_index &&
            edge->target_index == target_index) {
            return COMTRACE_GRAPH_SUCCESS;
        }
    }

    if (!ensure_edge_capacity(graph)) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }

    edge = &graph->edges[graph->edge_count];
    edge->type = type;
    edge->source_index = source_index;
    edge->target_index = target_index;
    ++graph->edge_count;
    return COMTRACE_GRAPH_SUCCESS;
}

static COMTRACE_GRAPH_STATUS add_process_with_generation(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    DWORD process_id,
    ULONGLONG process_generation,
    BOOL has_process_generation,
    size_t *index
)
{
    return add_node_with_generation(
        graph,
        COMTRACE_GRAPH_NODE_PROCESS,
        process_id,
        0U,
        NULL,
        0ULL,
        process_generation,
        has_process_generation,
        index
    );
}

static COMTRACE_GRAPH_STATUS add_thread_with_generation(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    DWORD process_id,
    DWORD thread_id,
    ULONGLONG process_generation,
    BOOL has_process_generation,
    size_t *index
)
{
    size_t process_index;
    COMTRACE_GRAPH_STATUS status;

    status = add_process_with_generation(
        graph,
        process_id,
        process_generation,
        has_process_generation,
        &process_index
    );
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }

    status = add_node_with_generation(
        graph,
        COMTRACE_GRAPH_NODE_THREAD,
        process_id,
        thread_id,
        NULL,
        0ULL,
        process_generation,
        has_process_generation,
        index
    );
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }

    return add_edge(
        graph,
        COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD,
        process_index,
        *index
    );
}

static void resolve_identity_process_generation(
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle,
    const COMTRACE_COM_OBJECT_IDENTITY *identity,
    DWORD process_id,
    ULONGLONG *generation,
    BOOL *has_generation
)
{
    *generation = 0ULL;
    *has_generation = FALSE;

    if (process_lifecycle == NULL ||
        identity == NULL ||
        !identity->has_timestamp ||
        process_id == 0U) {
        return;
    }

    *has_generation = comtrace_process_lifecycle_resolve_generation(
        process_lifecycle,
        process_id,
        identity->timestamp,
        generation
    );
}

static void record_identity_observation(
    COMTRACE_GRAPH_NODE *node,
    const COMTRACE_COM_OBJECT_IDENTITY *identity
)
{
    if (node == NULL || identity == NULL) {
        return;
    }

    if (node->observation_count != (ULONGLONG)-1) {
        ++node->observation_count;
    }

    if (!identity->has_timestamp) {
        return;
    }

    if (!node->has_observed_timestamp) {
        node->first_seen_timestamp = identity->timestamp;
        node->last_seen_timestamp = identity->timestamp;
        node->has_observed_timestamp = TRUE;
        return;
    }

    if (identity->timestamp < node->first_seen_timestamp) {
        node->first_seen_timestamp = identity->timestamp;
    }

    if (identity->timestamp > node->last_seen_timestamp) {
        node->last_seen_timestamp = identity->timestamp;
    }
}

static COMTRACE_GRAPH_STATUS add_call_relationship(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_COM_OBJECT_IDENTITY *identity,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle
)
{
    DWORD caller_process_id = 0U;
    DWORD caller_thread_id = 0U;
    DWORD target_process_id = 0U;
    DWORD target_thread_id = 0U;
    DWORD event_process_id;
    DWORD event_thread_id;
    ULONGLONG caller_generation = 0ULL;
    ULONGLONG target_generation = 0ULL;
    BOOL has_caller_generation = FALSE;
    BOOL has_target_generation = FALSE;
    size_t call_index;
    size_t caller_process_index = (size_t)-1;
    size_t caller_thread_index = (size_t)-1;
    size_t target_process_index = (size_t)-1;
    size_t target_thread_index = (size_t)-1;
    COMTRACE_GRAPH_STATUS status;

    if (!identity->has_call_trace_id) {
        return COMTRACE_GRAPH_SUCCESS;
    }

    event_process_id = identity->has_process_id ? identity->process_id : 0U;
    event_thread_id = identity->has_thread_id ? identity->thread_id : 0U;

    if (identity->has_source_process_id && identity->source_process_id != 0U &&
        identity->has_target_process_id && identity->target_process_id != 0U) {
        caller_process_id = identity->source_process_id;
        target_process_id = identity->target_process_id;
        caller_thread_id = identity->has_source_thread_id
            ? identity->source_thread_id
            : 0U;
        target_thread_id = identity->has_target_thread_id
            ? identity->target_thread_id
            : 0U;

        if (caller_thread_id == 0U && event_process_id == caller_process_id) {
            caller_thread_id = event_thread_id;
        }

        if (target_thread_id == 0U && event_process_id == target_process_id) {
            target_thread_id = event_thread_id;
        }
    } else if (identity->has_target_process_id && identity->target_process_id != 0U) {
        caller_process_id = event_process_id;
        caller_thread_id = event_thread_id;
        target_process_id = identity->target_process_id;
        target_thread_id = identity->has_target_thread_id
            ? identity->target_thread_id
            : 0U;
    } else if (identity->has_source_process_id && identity->source_process_id != 0U) {
        caller_process_id = identity->source_process_id;
        caller_thread_id = identity->has_source_thread_id
            ? identity->source_thread_id
            : 0U;
        target_process_id = event_process_id;
        target_thread_id = event_thread_id;
    } else {
        caller_process_id = event_process_id;
        caller_thread_id = event_thread_id;
    }

    resolve_identity_process_generation(
        process_lifecycle,
        identity,
        caller_process_id,
        &caller_generation,
        &has_caller_generation
    );
    resolve_identity_process_generation(
        process_lifecycle,
        identity,
        target_process_id,
        &target_generation,
        &has_target_generation
    );

    status = add_node(
        graph,
        COMTRACE_GRAPH_NODE_CALL,
        0U,
        0U,
        NULL,
        identity->call_trace_id,
        &call_index
    );
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }

    if (caller_process_id != 0U) {
        status = add_process_with_generation(
            graph,
            caller_process_id,
            caller_generation,
            has_caller_generation,
            &caller_process_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }

        if (caller_thread_id != 0U) {
            status = add_thread_with_generation(
                graph,
                caller_process_id,
                caller_thread_id,
                caller_generation,
                has_caller_generation,
                &caller_thread_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }

    if (target_process_id != 0U) {
        status = add_process_with_generation(
            graph,
            target_process_id,
            target_generation,
            has_target_generation,
            &target_process_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }

        if (target_thread_id != 0U) {
            status = add_thread_with_generation(
                graph,
                target_process_id,
                target_thread_id,
                target_generation,
                has_target_generation,
                &target_thread_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }

    if (caller_process_index != (size_t)-1 &&
        target_process_index != (size_t)-1) {
        status = add_edge(
            graph,
            COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS,
            caller_process_index,
            target_process_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (caller_thread_index != (size_t)-1) {
        status = add_edge(
            graph,
            COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL,
            caller_thread_index,
            call_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (target_thread_index != (size_t)-1) {
        status = add_edge(
            graph,
            COMTRACE_GRAPH_EDGE_CALL_TARGETS_THREAD,
            call_index,
            target_thread_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    return COMTRACE_GRAPH_SUCCESS;
}

void comtrace_relationship_graph_init(COMTRACE_RELATIONSHIP_GRAPH *graph)
{
    if (graph != NULL) {
        ZeroMemory(graph, sizeof(*graph));
    }
}

void comtrace_relationship_graph_destroy(COMTRACE_RELATIONSHIP_GRAPH *graph)
{
    if (graph == NULL) {
        return;
    }

    free(graph->nodes);
    free(graph->edges);
    ZeroMemory(graph, sizeof(*graph));
}

static COMTRACE_GRAPH_STATUS add_event_internal(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_COM_OBJECT_IDENTITY *identity,
    const COMTRACE_ACTIVITY_IDS *activity_ids,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle
)
{
    size_t current_process_index = (size_t)-1;
    size_t current_thread_index = (size_t)-1;
    size_t activity_index = (size_t)-1;
    size_t related_activity_index = (size_t)-1;
    size_t call_index;
    size_t object_index;
    size_t class_index;
    size_t iid_index;
    size_t ipid_index;
    DWORD object_process_id;
    ULONGLONG current_process_generation = 0ULL;
    ULONGLONG object_process_generation = 0ULL;
    BOOL has_current_process_generation = FALSE;
    BOOL has_object_process_generation = FALSE;
    COMTRACE_GRAPH_STATUS status;

    if (graph == NULL || identity == NULL) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }

    resolve_identity_process_generation(
        process_lifecycle,
        identity,
        identity->has_process_id ? identity->process_id : 0U,
        &current_process_generation,
        &has_current_process_generation
    );

    if (identity->has_process_id && identity->process_id != 0U) {
        status = add_process_with_generation(
            graph,
            identity->process_id,
            current_process_generation,
            has_current_process_generation,
            &current_process_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (identity->has_process_id && identity->process_id != 0U &&
        identity->has_thread_id && identity->thread_id != 0U) {
        status = add_thread_with_generation(
            graph,
            identity->process_id,
            identity->thread_id,
            current_process_generation,
            has_current_process_generation,
            &current_thread_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (activity_ids != NULL) {
        if (activity_ids->has_activity_id) {
            status = add_node(
                graph,
                COMTRACE_GRAPH_NODE_ACTIVITY,
                0U,
                0U,
                &activity_ids->activity_id,
                0ULL,
                &activity_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }

            if (current_thread_index != (size_t)-1) {
                status = add_edge(
                    graph,
                    COMTRACE_GRAPH_EDGE_THREAD_HAS_ACTIVITY,
                    current_thread_index,
                    activity_index
                );
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    return status;
                }
            }

            if (identity->has_call_trace_id) {
                status = add_node(
                    graph,
                    COMTRACE_GRAPH_NODE_CALL,
                    0U,
                    0U,
                    NULL,
                    identity->call_trace_id,
                    &call_index
                );
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    return status;
                }

                status = add_edge(
                    graph,
                    COMTRACE_GRAPH_EDGE_CALL_HAS_ACTIVITY,
                    call_index,
                    activity_index
                );
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    return status;
                }
            }
        }

        if (activity_ids->has_related_activity_id) {
            status = add_node(
                graph,
                COMTRACE_GRAPH_NODE_ACTIVITY,
                0U,
                0U,
                &activity_ids->related_activity_id,
                0ULL,
                &related_activity_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }

            if (activity_ids->has_activity_id &&
                memcmp(
                    &activity_ids->activity_id,
                    &activity_ids->related_activity_id,
                    sizeof(GUID)
                ) != 0) {
                status = add_edge(
                    graph,
                    COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO,
                    activity_index,
                    related_activity_index
                );
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    return status;
                }
            }
        }
    }

    if (identity->has_call_trace_id) {
        status = add_call_relationship(graph, identity, process_lifecycle);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    object_index = (size_t)-1;
    class_index = (size_t)-1;
    iid_index = (size_t)-1;
    ipid_index = (size_t)-1;

    if (identity->has_clsid) {
        status = add_node(
            graph,
            COMTRACE_GRAPH_NODE_CLSID,
            0U,
            0U,
            &identity->clsid,
            0ULL,
            &class_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (identity->has_iid) {
        status = add_node(
            graph,
            COMTRACE_GRAPH_NODE_IID,
            0U,
            0U,
            &identity->iid,
            0ULL,
            &iid_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (identity->has_ipid) {
        status = add_node(
            graph,
            COMTRACE_GRAPH_NODE_IPID,
            0U,
            0U,
            &identity->ipid,
            0ULL,
            &ipid_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }

        record_identity_observation(&graph->nodes[ipid_index], identity);

        if (iid_index != (size_t)-1) {
            status = add_edge(
                graph,
                COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID,
                ipid_index,
                iid_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }

    object_process_id = identity->has_target_process_id && identity->target_process_id != 0U
        ? identity->target_process_id
        : (identity->has_process_id && identity->process_id != 0U
            ? identity->process_id
            : 0U);

    resolve_identity_process_generation(
        process_lifecycle,
        identity,
        object_process_id,
        &object_process_generation,
        &has_object_process_generation
    );

    if (identity->has_oid && object_process_id != 0U) {
        size_t owner_process_index;

        status = add_process_with_generation(
            graph,
            object_process_id,
            object_process_generation,
            has_object_process_generation,
            &owner_process_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }

        status = add_node_with_generation(
            graph,
            COMTRACE_GRAPH_NODE_COM_OBJECT,
            object_process_id,
            0U,
            NULL,
            identity->oid,
            object_process_generation,
            has_object_process_generation,
            &object_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }

        record_identity_observation(&graph->nodes[object_index], identity);

        status = add_edge(
            graph,
            COMTRACE_GRAPH_EDGE_PROCESS_OWNS_OBJECT,
            owner_process_index,
            object_index
        );
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }

        if (identity->has_call_trace_id) {
            status = add_node(
                graph,
                COMTRACE_GRAPH_NODE_CALL,
                0U,
                0U,
                NULL,
                identity->call_trace_id,
                &call_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }

            status = add_edge(
                graph,
                COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT,
                call_index,
                object_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }

        if (class_index != (size_t)-1) {
            status = add_edge(
                graph,
                COMTRACE_GRAPH_EDGE_OBJECT_HAS_CLSID,
                object_index,
                class_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }

        if (ipid_index != (size_t)-1) {
            status = add_edge(
                graph,
                COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID,
                object_index,
                ipid_index
            );
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }

    return COMTRACE_GRAPH_SUCCESS;
}


COMTRACE_GRAPH_STATUS comtrace_relationship_graph_add_event_with_lifecycle(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_COM_OBJECT_IDENTITY *identity,
    const COMTRACE_ACTIVITY_IDS *activity_ids,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle
)
{
    return add_event_internal(
        graph,
        identity,
        activity_ids,
        process_lifecycle
    );
}

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_add_event(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_COM_OBJECT_IDENTITY *identity,
    const COMTRACE_ACTIVITY_IDS *activity_ids
)
{
    return add_event_internal(graph, identity, activity_ids, NULL);
}

size_t comtrace_relationship_graph_node_count(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_NODE_TYPE type
)
{
    size_t count = 0U;
    size_t index;

    if (graph == NULL) {
        return 0U;
    }

    for (index = 0U; index < graph->node_count; ++index) {
        if (graph->nodes[index].type == type) {
            ++count;
        }
    }

    return count;
}

size_t comtrace_relationship_graph_edge_count(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_EDGE_TYPE type
)
{
    size_t count = 0U;
    size_t index;

    if (graph == NULL) {
        return 0U;
    }

    for (index = 0U; index < graph->edge_count; ++index) {
        if (graph->edges[index].type == type) {
            ++count;
        }
    }

    return count;
}

const COMTRACE_GRAPH_NODE *comtrace_relationship_graph_get_node(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t index
)
{
    if (graph == NULL || index >= graph->node_count) {
        return NULL;
    }

    return &graph->nodes[index];
}

const COMTRACE_GRAPH_EDGE *comtrace_relationship_graph_get_edge(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t index
)
{
    if (graph == NULL || index >= graph->edge_count) {
        return NULL;
    }

    return &graph->edges[index];
}
