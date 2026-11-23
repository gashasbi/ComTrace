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
        return COMTRACE_GRAPH_INVALID_ARGUMENT;
    }
    if (record->has_start && process_lifecycle != NULL) {
        has_generation = comtrace_process_lifecycle_resolve_generation(
            process_lifecycle, record->process_id,
            (LONGLONG)record->start_timestamp, &generation);
    }
    status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_RPC_CALL,
        record->process_id, 0U, NULL, (ULONGLONG)record_index + 1ULL,
        rpc_node_index);
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }
    graph->nodes[*rpc_node_index].process_generation = generation;
    graph->nodes[*rpc_node_index].has_process_generation = has_generation;

    status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_PROCESS,
        record->process_id, 0U, NULL, 0ULL, &process_index);
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }
    status = add_protocol_edge(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_RPC_CALL,
        process_index, *rpc_node_index);
    if (status != COMTRACE_GRAPH_SUCCESS) {
        return status;
    }

    if (record->thread_id != 0U) {
        status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_THREAD,
            record->process_id, record->thread_id, NULL, 0ULL, &thread_index);
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
        COMTRACE_GRAPH_STATUS status;
        transaction_nodes[index] = (size_t)-1;

        if (record->process_id == 0U) {
            continue;
        }
        status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_PROCESS,
            record->process_id, 0U, NULL, 0ULL, &process_index);
        if (status != COMTRACE_GRAPH_SUCCESS) {
            free(transaction_nodes);
            return status;
        }
        if (record->conversation_id != 0ULL) {
            status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_DDE_CONVERSATION,
                record->process_id, 0U, NULL, record->conversation_id, &conversation_index);
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
            status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_DDE_TRANSACTION,
                record->process_id, 0U, NULL, record->transaction_instance_id, &transaction_index);
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
                status = add_protocol_node(graph, COMTRACE_GRAPH_NODE_THREAD,
                    record->process_id, record->thread_id, NULL, 0ULL, &thread_index);
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
