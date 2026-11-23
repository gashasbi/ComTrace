#include "relationship_graph.h"

#include <stdlib.h>
#include <string.h>

static int ensure_nodes(COMTRACE_RELATIONSHIP_GRAPH *graph)
{
    size_t capacity;
    COMTRACE_GRAPH_NODE *nodes;

    if (graph->node_count < graph->node_capacity) {
        return 1;
    }
    capacity = graph->node_capacity == 0U ? 16U : graph->node_capacity * 2U;
    if (capacity < graph->node_capacity ||
        capacity > (size_t)-1 / sizeof(*nodes)) {
        return 0;
    }
    nodes = (COMTRACE_GRAPH_NODE *)realloc(graph->nodes, capacity * sizeof(*nodes));
    if (nodes == NULL) {
        return 0;
    }
    graph->nodes = nodes;
    graph->node_capacity = capacity;
    return 1;
}

static int ensure_edges(COMTRACE_RELATIONSHIP_GRAPH *graph)
{
    size_t capacity;
    COMTRACE_GRAPH_EDGE *edges;

    if (graph->edge_count < graph->edge_capacity) {
        return 1;
    }
    capacity = graph->edge_capacity == 0U ? 32U : graph->edge_capacity * 2U;
    if (capacity < graph->edge_capacity ||
        capacity > (size_t)-1 / sizeof(*edges)) {
        return 0;
    }
    edges = (COMTRACE_GRAPH_EDGE *)realloc(graph->edges, capacity * sizeof(*edges));
    if (edges == NULL) {
        return 0;
    }
    graph->edges = edges;
    graph->edge_capacity = capacity;
    return 1;
}

static COMTRACE_GRAPH_STATUS add_protocol_node(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_NODE_TYPE type,
    DWORD process_id,
    DWORD thread_id,
    const GUID *guid,
    ULONGLONG value,
    size_t *node_index
)
{
    size_t index;
    COMTRACE_GRAPH_NODE *node;

    for (index = 0U; index < graph->node_count; ++index) {
        node = &graph->nodes[index];
        if (node->type != type || node->process_id != process_id ||
            node->thread_id != thread_id || node->value != value ||
            node->has_guid != (guid != NULL)) {
            continue;
        }
        if (guid != NULL && memcmp(&node->guid, guid, sizeof(*guid)) != 0) {
            continue;
        }
        *node_index = index;
        return COMTRACE_GRAPH_SUCCESS;
    }

    if (!ensure_nodes(graph)) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
    index = graph->node_count++;
    node = &graph->nodes[index];
    ZeroMemory(node, sizeof(*node));
    node->type = type;
    node->process_id = process_id;
    node->thread_id = thread_id;
    node->value = value;
    if (guid != NULL) {
        node->guid = *guid;
        node->has_guid = TRUE;
    }
    *node_index = index;
    return COMTRACE_GRAPH_SUCCESS;
}

static COMTRACE_GRAPH_STATUS add_scoped_node(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_NODE_TYPE type,
    DWORD process_id,
    DWORD thread_id,
    const GUID *guid,
    ULONGLONG value,
    ULONGLONG process_generation,
    BOOL has_process_generation,
    size_t *node_index
)
{
    size_t index;
    COMTRACE_GRAPH_NODE *node;

    for (index = 0U; index < graph->node_count; ++index) {
        node = &graph->nodes[index];
        if (node->type != type || node->process_id != process_id ||
            node->thread_id != thread_id || node->value != value ||
            node->has_guid != (guid != NULL) ||
            node->has_process_generation != has_process_generation ||
            (has_process_generation &&
             node->process_generation != process_generation)) {
            continue;
        }
        if (guid != NULL && memcmp(&node->guid, guid, sizeof(*guid)) != 0) {
            continue;
        }
        *node_index = index;
        return COMTRACE_GRAPH_SUCCESS;
    }

    if (!ensure_nodes(graph)) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
    index = graph->node_count++;
    node = &graph->nodes[index];
    ZeroMemory(node, sizeof(*node));
    node->type = type;
    node->process_id = process_id;
    node->thread_id = thread_id;
    node->process_generation = process_generation;
    node->has_process_generation = has_process_generation;
    node->value = value;
    if (guid != NULL) {
        node->guid = *guid;
        node->has_guid = TRUE;
    }
    *node_index = index;
    return COMTRACE_GRAPH_SUCCESS;
}

static COMTRACE_GRAPH_STATUS add_protocol_edge(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_EDGE_TYPE type,
    size_t source,
    size_t target
)
{
    size_t index;
    COMTRACE_GRAPH_EDGE *edge;

    if (source >= graph->node_count || target >= graph->node_count) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    for (index = 0U; index < graph->edge_count; ++index) {
        edge = &graph->edges[index];
        if (edge->type == type && edge->source_index == source &&
            edge->target_index == target) {
            return COMTRACE_GRAPH_SUCCESS;
        }
    }
    if (!ensure_edges(graph)) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
    edge = &graph->edges[graph->edge_count++];
    edge->type = type;
    edge->source_index = source;
    edge->target_index = target;
    return COMTRACE_GRAPH_SUCCESS;
}

static COMTRACE_GRAPH_STATUS add_rpc_record(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_RPC_TIMELINE_RECORD *record,
    size_t record_index,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle,
    size_t *rpc_node_index
)
{
    size_t process_index;
    size_t thread_index;
    size_t interface_index;
    size_t activity_index;
    ULONGLONG generation = 0ULL;
    BOOL has_generation = FALSE;
    COMTRACE_GRAPH_STATUS status;

    if (record->process_id == 0U || (!record->has_start && !record->has_stop)) {
        return COMTRACE_GRAPH_SUCCESS;
    }
    if (process_lifecycle != NULL) {
        LONGLONG timestamp = (LONGLONG)(record->has_start
            ? record->start_timestamp
            : record->stop_timestamp);
        has_generation = comtrace_process_lifecycle_resolve_generation(
            process_lifecycle, record->process_id, timestamp, &generation);
    }
    status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_RPC_CALL,
        record->process_id, 0U, NULL, (ULONGLONG)record_index + 1ULL,
        generation, has_generation, rpc_node_index);
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }
    status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_PROCESS,
        record->process_id, 0U, NULL, 0ULL,
        generation, has_generation, &process_index);
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }
    status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_RPC_CALL,
        process_index, *rpc_node_index);
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }

    if (record->thread_id != 0U) {
        status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_THREAD,
            record->process_id, record->thread_id, NULL, 0ULL,
            generation, has_generation, &thread_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD,
            process_index, thread_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_RPC_CALL,
            thread_index, *rpc_node_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (record->has_interface_uuid) {
        status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_RPC_INTERFACE,
            0U, 0U, &record->interface_uuid, 0ULL, &interface_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_RPC_CALL_USES_INTERFACE,
            *rpc_node_index, interface_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }

    if (record->has_activity_id) {
        status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_ACTIVITY,
            0U, 0U, &record->activity_id, 0ULL, &activity_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_RPC_CALL_HAS_ACTIVITY,
            *rpc_node_index, activity_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
    }
    return COMTRACE_GRAPH_SUCCESS;
}

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_add_rpc_timeline(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_RPC_TIMELINE *timeline,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle
)
{
    size_t index;
    size_t *node_indices;

    if (graph == NULL || timeline == NULL) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    if (timeline->record_count == 0U) {
        return COMTRACE_GRAPH_SUCCESS;
    }
    if (timeline->record_count > (size_t)-1 / sizeof(*node_indices)) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
    node_indices = (size_t *)malloc(timeline->record_count * sizeof(*node_indices));
    if (node_indices == NULL) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
    for (index = 0U; index < timeline->record_count; ++index) {
        const COMTRACE_RPC_TIMELINE_RECORD *record = &timeline->records[index];
        COMTRACE_GRAPH_STATUS status;
        node_indices[index] = (size_t)-1;
        if (!record->has_start && !record->has_stop) {
            continue;
        }
        status = add_rpc_record(graph, record, index, process_lifecycle,
            &node_indices[index]);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            free(node_indices);
            return status;
        }
    }
    for (index = 0U; index < timeline->record_count; ++index) {
        const COMTRACE_RPC_TIMELINE_RECORD *record = &timeline->records[index];
        if (!record->has_correlated_record ||
            record->correlated_record_index >= timeline->record_count ||
            node_indices[index] == (size_t)-1 ||
            node_indices[record->correlated_record_index] == (size_t)-1 ||
            index >= record->correlated_record_index) {
            continue;
        }
        {
            COMTRACE_GRAPH_STATUS status = add_protocol_edge(graph,
                COMTRACE_GRAPH_EDGE_RPC_CALL_CORRELATES_WITH_RPC_CALL,
                node_indices[index],
                node_indices[record->correlated_record_index]);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                free(node_indices);
                return status;
            }
        }
    }
    free(node_indices);
    return COMTRACE_GRAPH_SUCCESS;
}

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_add_dde_timeline(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_DDE_TIMELINE *timeline,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle
)
{
    size_t index;
    size_t *transaction_nodes;

    (void)process_lifecycle;
    if (graph == NULL || timeline == NULL) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    if (timeline->record_count == 0U) {
        return COMTRACE_GRAPH_SUCCESS;
    }
    if (timeline->record_count > (size_t)-1 / sizeof(*transaction_nodes)) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
    transaction_nodes = (size_t *)malloc(timeline->record_count * sizeof(*transaction_nodes));
    if (transaction_nodes == NULL) {
        return COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
    for (index = 0U; index < timeline->record_count; ++index) {
        const COMTRACE_DDE_TIMELINE_RECORD *record = &timeline->records[index];
        size_t process_index;
        size_t conversation_index = (size_t)-1;
        size_t transaction_index = (size_t)-1;
        size_t correlation_index;
        ULONGLONG generation = 0ULL;
        BOOL has_generation = FALSE;
        LONGLONG timestamp;
        COMTRACE_GRAPH_STATUS status;
        transaction_nodes[index] = (size_t)-1;

        if (record->process_id == 0U) {
            continue;
        }
        timestamp = (LONGLONG)(record->has_start ? record->start_timestamp :
            (record->has_opened ? record->opened_timestamp :
            (record->has_completion ? record->completion_timestamp :
             record->callback_timestamp)));
        if (process_lifecycle != NULL) {
            has_generation = comtrace_process_lifecycle_resolve_generation(
                process_lifecycle, record->process_id, timestamp, &generation);
        }
        status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_PROCESS,
            record->process_id, 0U, NULL, 0ULL,
            generation, has_generation, &process_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            free(transaction_nodes);
            return status;
        }
        if (record->conversation_id != 0ULL) {
            status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_DDE_CONVERSATION,
                record->process_id, 0U, NULL, record->conversation_id,
                generation, has_generation, &conversation_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                free(transaction_nodes);
                return status;
            }
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_DDE_CONVERSATION,
                process_index, conversation_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                free(transaction_nodes);
                return status;
            }
            if (record->conversation_correlation_id != 0ULL) {
                status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_DDE_CONVERSATION_CORRELATION,
                    0U, 0U, NULL, record->conversation_correlation_id, &correlation_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
                status = add_protocol_edge(graph,
                    COMTRACE_GRAPH_EDGE_DDE_CONVERSATION_CORRELATES_WITH,
                    conversation_index, correlation_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
            }
        }
        if (record->transaction_instance_id != 0ULL) {
            status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_DDE_TRANSACTION,
                record->process_id, 0U, NULL, record->transaction_instance_id,
                generation, has_generation, &transaction_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                free(transaction_nodes);
                return status;
            }
            transaction_nodes[index] = transaction_index;
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_DDE_TRANSACTION,
                process_index, transaction_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                free(transaction_nodes);
                return status;
            }
            if (conversation_index != (size_t)-1) {
                status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_DDE_TRANSACTION_IN_CONVERSATION,
                    transaction_index, conversation_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
            }
            if (record->thread_id != 0U) {
                size_t thread_index;
                status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_THREAD,
                    record->process_id, record->thread_id, NULL, 0ULL,
                    generation, has_generation, &thread_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
                status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD,
                    process_index, thread_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
                status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_DDE_TRANSACTION,
                    thread_index, transaction_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
            }
            if (record->transaction_correlation_id != 0ULL) {
                status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_DDE_TRANSACTION_CORRELATION,
                    0U, 0U, NULL, record->transaction_correlation_id, &correlation_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
                status = add_protocol_edge(graph,
                    COMTRACE_GRAPH_EDGE_DDE_TRANSACTION_HAS_CORRELATION,
                    transaction_index, correlation_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    free(transaction_nodes);
                    return status;
                }
            }
        }
    }
    for (index = 0U; index < timeline->exchange_count; ++index) {
        const COMTRACE_DDE_EXCHANGE *exchange = &timeline->exchanges[index];
        size_t client;
        size_t server;
        COMTRACE_GRAPH_STATUS status;
        if (exchange->client_record_index >= timeline->record_count ||
            exchange->server_record_index >= timeline->record_count) {
            continue;
        }
        client = transaction_nodes[exchange->client_record_index];
        server = transaction_nodes[exchange->server_record_index];
        if (client == (size_t)-1 || server == (size_t)-1) {
            continue;
        }
        status = add_protocol_edge(graph,
            COMTRACE_GRAPH_EDGE_DDE_TRANSACTION_EXCHANGES_WITH,
            client, server);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            free(transaction_nodes);
            return status;
        }
    }
    free(transaction_nodes);
    return COMTRACE_GRAPH_SUCCESS;
}

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_add_ole_timeline(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const COMTRACE_OLE_TIMELINE *timeline,
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *process_lifecycle
)
{
    size_t index;

    if (graph == NULL || timeline == NULL) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    for (index = 0U; index < timeline->record_count; ++index) {
        const COMTRACE_OLE_TIMELINE_RECORD *record = &timeline->records[index];
        size_t operation_index;
        size_t process_index;
        size_t thread_index;
        size_t activity_index;
        ULONGLONG generation = 0ULL;
        BOOL has_generation = FALSE;
        COMTRACE_GRAPH_STATUS status;

        if (record->process_id == 0U || (!record->has_start && !record->has_stop)) {
            continue;
        }
        if (process_lifecycle != NULL) {
            LONGLONG timestamp = (LONGLONG)(record->has_start
                ? record->start_timestamp
                : record->stop_timestamp);
            has_generation = comtrace_process_lifecycle_resolve_generation(
                process_lifecycle, record->process_id, timestamp, &generation);
        }
        status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_OLE_OPERATION,
            record->process_id, 0U,
            record->has_activity_id ? &record->activity_id : NULL,
            (ULONGLONG)index + 1ULL, generation, has_generation, &operation_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }

        status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_PROCESS,
            record->process_id, 0U, NULL, 0ULL,
            generation, has_generation, &process_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_OLE_OPERATION,
            process_index, operation_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        if (record->thread_id != 0U) {
            status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_THREAD,
                record->process_id, record->thread_id, NULL, 0ULL,
                generation, has_generation, &thread_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD,
                process_index, thread_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_OLE_OPERATION,
                thread_index, operation_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
        if (record->has_activity_id) {
            status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_ACTIVITY,
                0U, 0U, &record->activity_id, 0ULL, &activity_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_OLE_OPERATION_HAS_ACTIVITY,
                operation_index, activity_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }
    return COMTRACE_GRAPH_SUCCESS;
}

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_link_ole_data_objects(
    COMTRACE_RELATIONSHIP_GRAPH *graph
)
{
    static const GUID data_object_iid = {
        0x0000010E, 0x0000, 0x0000,
        {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    size_t operation_index;

    if (graph == NULL) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    for (operation_index = 0U; operation_index < graph->node_count; ++operation_index) {
        const COMTRACE_GRAPH_NODE *operation = &graph->nodes[operation_index];
        size_t activity_index;
        size_t match_index = (size_t)-1;
        size_t match_count = 0U;

        if (operation->type != COMTRACE_GRAPH_NODE_OLE_OPERATION ||
            !operation->has_guid) {
            continue;
        }
        for (activity_index = 0U; activity_index < graph->node_count; ++activity_index) {
            const COMTRACE_GRAPH_NODE *activity = &graph->nodes[activity_index];
            size_t edge_index;

            if (activity->type != COMTRACE_GRAPH_NODE_ACTIVITY ||
                !activity->has_guid ||
                memcmp(&activity->guid, &operation->guid, sizeof(GUID)) != 0) {
                continue;
            }
            for (edge_index = 0U; edge_index < graph->edge_count; ++edge_index) {
                const COMTRACE_GRAPH_EDGE *edge = &graph->edges[edge_index];
                size_t object_index;

                if (edge->type == COMTRACE_GRAPH_EDGE_CALL_HAS_ACTIVITY &&
                    edge->target_index == activity_index) {
                    size_t object_edge_index;
                    for (object_edge_index = 0U;
                         object_edge_index < graph->edge_count;
                         ++object_edge_index) {
                        const COMTRACE_GRAPH_EDGE *object_edge =
                            &graph->edges[object_edge_index];
                        int is_data_object = 0;

                        if (object_edge->type != COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT ||
                            object_edge->source_index != edge->source_index) {
                            continue;
                        }
                        object_index = object_edge->target_index;
                        {
                            size_t iid_edge_index;
                            for (iid_edge_index = 0U;
                                 iid_edge_index < graph->edge_count;
                                 ++iid_edge_index) {
                                const COMTRACE_GRAPH_EDGE *iid_edge =
                                    &graph->edges[iid_edge_index];
                                const COMTRACE_GRAPH_NODE *iid_node;
                                if (iid_edge->source_index != object_index ||
                                    (iid_edge->type != COMTRACE_GRAPH_EDGE_OBJECT_EXPOSES_IID &&
                                     iid_edge->type != COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID)) {
                                    continue;
                                }
                                if (iid_edge->type == COMTRACE_GRAPH_EDGE_OBJECT_EXPOSES_IID) {
                                    iid_node = &graph->nodes[iid_edge->target_index];
                                    if (iid_node->type == COMTRACE_GRAPH_NODE_IID &&
                                        memcmp(&iid_node->guid, &data_object_iid, sizeof(GUID)) == 0) {
                                        is_data_object = 1;
                                    }
                                } else {
                                    size_t supports_index;
                                    for (supports_index = 0U;
                                         supports_index < graph->edge_count;
                                         ++supports_index) {
                                        const COMTRACE_GRAPH_EDGE *supports =
                                            &graph->edges[supports_index];
                                        if (supports->type != COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID ||
                                            supports->source_index != iid_edge->target_index) {
                                            continue;
                                        }
                                        iid_node = &graph->nodes[supports->target_index];
                                        if (iid_node->type == COMTRACE_GRAPH_NODE_IID &&
                                            memcmp(&iid_node->guid, &data_object_iid, sizeof(GUID)) == 0) {
                                            is_data_object = 1;
                                        }
                                    }
                                }
                            }
                        }
                        if (is_data_object) {
                            match_index = object_index;
                            ++match_count;
                        }
                    }
                }
            }
            for (edge_index = 0U; edge_index < graph->edge_count; ++edge_index) {
                const COMTRACE_GRAPH_EDGE *edge = &graph->edges[edge_index];
                const COMTRACE_GRAPH_NODE *object;
                size_t iid_edge_index;
                int is_data_object = 0;

                if (edge->type != COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_HAS_ACTIVITY ||
                    edge->target_index != activity_index) {
                    continue;
                }
                object_index = edge->source_index;
                for (iid_edge_index = 0U; iid_edge_index < graph->edge_count; ++iid_edge_index) {
                    const COMTRACE_GRAPH_EDGE *iid_edge = &graph->edges[iid_edge_index];
                    if (iid_edge->type != COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_EXPOSES_IID ||
                        iid_edge->source_index != object_index) {
                        continue;
                    }
                    object = &graph->nodes[iid_edge->target_index];
                    if (object->type == COMTRACE_GRAPH_NODE_IID &&
                        memcmp(&object->guid, &data_object_iid, sizeof(GUID)) == 0) {
                        is_data_object = 1;
                    }
                }
                if (is_data_object) {
                    match_index = object_index;
                    ++match_count;
                }
            }
        }
        if (match_count == 1U) {
            COMTRACE_GRAPH_STATUS status = add_protocol_edge(graph,
                COMTRACE_GRAPH_EDGE_OLE_OPERATION_USES_DATA_OBJECT,
                operation_index, match_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }
    return COMTRACE_GRAPH_SUCCESS;
}
