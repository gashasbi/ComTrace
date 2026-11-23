#include "relationship_graph.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

static void record_protocol_observation(
    COMTRACE_GRAPH_NODE *node,
    LONGLONG timestamp
)
{
    if (node == NULL) {
        return;
    }

    if (!node->has_observed_timestamp) {
        node->first_seen_timestamp = timestamp;
        node->last_seen_timestamp = timestamp;
        node->has_observed_timestamp = TRUE;
        return;
    }

    if (timestamp < node->first_seen_timestamp) {
        node->first_seen_timestamp = timestamp;
    }
    if (timestamp > node->last_seen_timestamp) {
        node->last_seen_timestamp = timestamp;
    }
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
        record->process_id, 0U,
        record->has_activity_id ? &record->activity_id : NULL,
        (ULONGLONG)record_index + 1ULL,
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

    if (record->has_stop && record->stop_process_id != 0U) {
        size_t stop_process_index;
        ULONGLONG stop_generation = 0ULL;
        BOOL has_stop_generation = FALSE;
        if (process_lifecycle != NULL) {
            has_stop_generation = comtrace_process_lifecycle_resolve_generation(
                process_lifecycle, record->stop_process_id,
                (LONGLONG)record->stop_timestamp, &stop_generation);
        }
        status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_PROCESS,
            record->stop_process_id, 0U, NULL, 0ULL,
            stop_generation, has_stop_generation, &stop_process_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_COMPLETES_RPC_CALL,
            stop_process_index, *rpc_node_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            return status;
        }
        if (record->stop_thread_id != 0U) {
            size_t stop_thread_index;
            status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_THREAD,
                record->stop_process_id, record->stop_thread_id, NULL, 0ULL,
                stop_generation, has_stop_generation, &stop_thread_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD,
                stop_process_index, stop_thread_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_THREAD_COMPLETES_RPC_CALL,
                stop_thread_index, *rpc_node_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }

    if (record->has_interface_uuid) {
        graph->nodes[*rpc_node_index].related_guid = record->interface_uuid;
        graph->nodes[*rpc_node_index].has_related_guid = TRUE;
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
            (record->has_closed ? record->closed_timestamp :
            (record->has_completion ? record->completion_timestamp :
             record->callback_timestamp))));
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
        if (record->operation[0] != L'\0') {
            size_t label_length = wcslen(record->operation);
            if (label_length >= _countof(graph->nodes[operation_index].label)) {
                label_length = _countof(graph->nodes[operation_index].label) - 1U;
            }
            memcpy(
                graph->nodes[operation_index].label,
                record->operation,
                label_length * sizeof(wchar_t)
            );
            graph->nodes[operation_index].label[label_length] = L'\0';
        }
        if (record->has_start) {
            record_protocol_observation(
                &graph->nodes[operation_index],
                (LONGLONG)record->start_timestamp
            );
        }
        if (record->has_stop) {
            record_protocol_observation(
                &graph->nodes[operation_index],
                (LONGLONG)record->stop_timestamp
            );
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
        if (record->has_stop && record->stop_process_id != 0U) {
            size_t stop_process_index;
            ULONGLONG stop_generation = 0ULL;
            BOOL has_stop_generation = FALSE;
            if (process_lifecycle != NULL) {
                has_stop_generation = comtrace_process_lifecycle_resolve_generation(
                    process_lifecycle, record->stop_process_id,
                    (LONGLONG)record->stop_timestamp, &stop_generation);
            }
            status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_PROCESS,
                record->stop_process_id, 0U, NULL, 0ULL,
                stop_generation, has_stop_generation, &stop_process_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OBSERVES_OLE_OPERATION,
                stop_process_index, operation_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            if (record->stop_thread_id != 0U) {
                size_t stop_thread_index;
                status = add_scoped_node(graph, COMTRACE_GRAPH_NODE_THREAD,
                    record->stop_process_id, record->stop_thread_id, NULL, 0ULL,
                    stop_generation, has_stop_generation, &stop_thread_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    return status;
                }
                status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD,
                    stop_process_index, stop_thread_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    return status;
                }
                status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_THREAD_COMPLETES_OLE_OPERATION,
                    stop_thread_index, operation_index);
                if (status != COMTRACE_GRAPH_SUCCESS) {
                    return status;
                }
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
        if (record->has_related_activity_id) {
            size_t related_activity_index;

            status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_ACTIVITY,
                0U, 0U, &record->related_activity_id, 0ULL,
                &related_activity_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
            if (record->has_activity_id &&
                memcmp(
                    &record->activity_id,
                    &record->related_activity_id,
                    sizeof(GUID)
                ) != 0) {
                status = add_protocol_edge(
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
    return COMTRACE_GRAPH_SUCCESS;
}

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_link_ole_data_objects(
    COMTRACE_RELATIONSHIP_GRAPH *graph
)
{
    size_t operation_index;

    if (graph == NULL) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    for (operation_index = 0U; operation_index < graph->node_count; ++operation_index) {
        const COMTRACE_GRAPH_NODE *operation = &graph->nodes[operation_index];
        size_t match_index = (size_t)-1;
        size_t match_count = 0U;
        size_t edge_index;

        if (operation->type != COMTRACE_GRAPH_NODE_OLE_OPERATION ||
            !operation->has_guid) {
            continue;
        }
        for (edge_index = 0U; edge_index < graph->edge_count; ++edge_index) {
            const COMTRACE_GRAPH_EDGE *edge = &graph->edges[edge_index];
            const COMTRACE_GRAPH_NODE *call;
            const COMTRACE_GRAPH_NODE *object;
            size_t candidate_index;

            if (edge->type != COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT) {
                continue;
            }
            call = &graph->nodes[edge->source_index];
            object = &graph->nodes[edge->target_index];
            if (call->type != COMTRACE_GRAPH_NODE_CALL ||
                !call->has_guid ||
                memcmp(&call->guid, &operation->guid, sizeof(GUID)) != 0 ||
                object->type != COMTRACE_GRAPH_NODE_COM_OBJECT ||
                !object->has_data_object_iid) {
                continue;
            }
            candidate_index = edge->target_index;
            if (match_index == (size_t)-1) {
                match_index = candidate_index;
                match_count = 1U;
            } else if (match_index != candidate_index) {
                match_count = 2U;
            }
        }
        for (edge_index = 0U; edge_index < graph->edge_count; ++edge_index) {
            const COMTRACE_GRAPH_EDGE *edge = &graph->edges[edge_index];
            const COMTRACE_GRAPH_NODE *activity;
            const COMTRACE_GRAPH_NODE *object;
            size_t candidate_index;

            if (edge->type != COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_HAS_ACTIVITY) {
                continue;
            }
            activity = &graph->nodes[edge->target_index];
            object = &graph->nodes[edge->source_index];
            if (activity->type != COMTRACE_GRAPH_NODE_ACTIVITY ||
                !activity->has_guid ||
                memcmp(&activity->guid, &operation->guid, sizeof(GUID)) != 0 ||
                object->type != COMTRACE_GRAPH_NODE_COM_LIFETIME_OBJECT ||
                !object->has_data_object_iid) {
                continue;
            }
            candidate_index = edge->source_index;
            if (match_index == (size_t)-1) {
                match_index = candidate_index;
                match_count = 1U;
            } else if (match_index != candidate_index) {
                match_count = 2U;
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

COMTRACE_GRAPH_STATUS comtrace_relationship_graph_link_rpc_com_calls(
    COMTRACE_RELATIONSHIP_GRAPH *graph
)
{
    size_t rpc_index;

    if (graph == NULL) {
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    for (rpc_index = 0U; rpc_index < graph->node_count; ++rpc_index) {
        const COMTRACE_GRAPH_NODE *rpc = &graph->nodes[rpc_index];
        size_t match_index = (size_t)-1;
        size_t match_count = 0U;
        size_t call_index;

        if (rpc->type != COMTRACE_GRAPH_NODE_RPC_CALL ||
            !rpc->has_guid || !rpc->has_related_guid) {
            continue;
        }
        for (call_index = 0U; call_index < graph->node_count; ++call_index) {
            const COMTRACE_GRAPH_NODE *call = &graph->nodes[call_index];
            if (call->type != COMTRACE_GRAPH_NODE_CALL ||
                !call->has_guid || !call->has_related_guid ||
                memcmp(&call->guid, &rpc->guid, sizeof(GUID)) != 0 ||
                memcmp(&call->related_guid, &rpc->related_guid, sizeof(GUID)) != 0) {
                continue;
            }
            match_index = call_index;
            ++match_count;
        }
        if (match_count == 1U) {
            COMTRACE_GRAPH_STATUS status = add_protocol_edge(graph,
                COMTRACE_GRAPH_EDGE_RPC_CALL_CORRELATES_WITH_COM_CALL,
                rpc_index, match_index);
            if (status != COMTRACE_GRAPH_SUCCESS) {
                return status;
            }
        }
    }
    return COMTRACE_GRAPH_SUCCESS;
}
