#include <stdio.h>
#include <string.h>
#include "relationship_graph.h"

static const GUID class_id = {
    0x00112233, 0x4455, 0x6677,
    { 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }
};

static const GUID interface_id = {
    0x10213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const GUID ipid = {
    0x40516273, 0x8495, 0xa6b7,
    { 0xc8, 0xd9, 0xea, 0xfb, 0x0c, 0x1d, 0x2e, 0x3f }
};

static const GUID second_ipid = {
    0x50516273, 0x8495, 0xa6b7,
    { 0xc8, 0xd9, 0xea, 0xfb, 0x0c, 0x1d, 0x2e, 0x3f }
};

static const GUID activity_id = {
    0x20314253, 0x6475, 0x8697,
    { 0xa8, 0xb9, 0xca, 0xdb, 0xec, 0xfd, 0x0e, 0x1f }
};

static const GUID related_activity_id = {
    0x30415263, 0x7485, 0x96a7,
    { 0xb8, 0xc9, 0xda, 0xeb, 0xfc, 0x0d, 0x1e, 0x2f }
};

static int has_process_call_direction(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    DWORD source_process_id,
    DWORD target_process_id
)
{
    size_t index;

    for (index = 0U; index < graph->edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *edge = &graph->edges[index];
        const COMTRACE_GRAPH_NODE *source;
        const COMTRACE_GRAPH_NODE *target;

        if (edge->type != COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS) {
            continue;
        }

        source = comtrace_relationship_graph_get_node(graph, edge->source_index);
        target = comtrace_relationship_graph_get_node(graph, edge->target_index);

        if (source != NULL &&
            target != NULL &&
            source->type == COMTRACE_GRAPH_NODE_PROCESS &&
            target->type == COMTRACE_GRAPH_NODE_PROCESS &&
            source->process_id == source_process_id &&
            target->process_id == target_process_id) {
            return 1;
        }
    }

    return 0;
}

static int has_thread_call_direction(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    COMTRACE_GRAPH_EDGE_TYPE edge_type,
    DWORD process_id,
    DWORD thread_id,
    ULONGLONG call_trace_id
)
{
    size_t index;

    for (index = 0U; index < graph->edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *edge = &graph->edges[index];
        const COMTRACE_GRAPH_NODE *thread;
        const COMTRACE_GRAPH_NODE *call;

        if (edge->type != edge_type) {
            continue;
        }

        if (edge_type == COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL) {
            thread = comtrace_relationship_graph_get_node(graph, edge->source_index);
            call = comtrace_relationship_graph_get_node(graph, edge->target_index);
        } else {
            call = comtrace_relationship_graph_get_node(graph, edge->source_index);
            thread = comtrace_relationship_graph_get_node(graph, edge->target_index);
        }

        if (thread != NULL &&
            call != NULL &&
            thread->type == COMTRACE_GRAPH_NODE_THREAD &&
            thread->process_id == process_id &&
            thread->thread_id == thread_id &&
            call->type == COMTRACE_GRAPH_NODE_CALL &&
            call->value == call_trace_id) {
            return 1;
        }
    }

    return 0;
}

static int test_client_server_graph(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    identity.process_id = 100U;
    identity.thread_id = 10U;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.target_process_id = 200U;
    identity.target_thread_id = 20U;
    identity.has_target_process_id = TRUE;
    identity.has_target_thread_id = TRUE;
    identity.has_call_trace_id = TRUE;
    identity.call_trace_id = 0xAABBCCDDULL;
    identity.has_oid = TRUE;
    identity.oid = 0x1122334455667788ULL;
    identity.has_clsid = TRUE;
    identity.clsid = class_id;
    identity.has_iid = TRUE;
    identity.iid = interface_id;
    identity.has_ipid = TRUE;
    identity.ipid = ipid;

    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id = activity_id;
    activity_ids.has_related_activity_id = TRUE;
    activity_ids.related_activity_id = related_activity_id;

    if (comtrace_relationship_graph_add_event(
            &graph,
            &identity,
            &activity_ids
        ) != COMTRACE_GRAPH_SUCCESS) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    if (comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_PROCESS) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_THREAD) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_ACTIVITY) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_CALL) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_COM_OBJECT) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_CLSID) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_IID) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_IPID) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_CALL_TARGETS_THREAD) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID) != 1U ||
        !has_process_call_direction(&graph, 100U, 200U)) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    identity.process_id = 200U;
    identity.thread_id = 20U;
    identity.has_target_process_id = FALSE;
    identity.has_target_thread_id = FALSE;
    identity.source_process_id = 100U;
    identity.source_thread_id = 10U;
    identity.has_source_process_id = TRUE;
    identity.has_source_thread_id = TRUE;

    if (comtrace_relationship_graph_add_event(
            &graph,
            &identity,
            &activity_ids
        ) != COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_PROCESS) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_THREAD) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_CALL) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS) != 1U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_explicit_source_and_target_processes(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));

    identity.process_id = 999U;
    identity.thread_id = 99U;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.source_process_id = 100U;
    identity.source_thread_id = 10U;
    identity.has_source_process_id = TRUE;
    identity.has_source_thread_id = TRUE;
    identity.target_process_id = 200U;
    identity.target_thread_id = 20U;
    identity.has_target_process_id = TRUE;
    identity.has_target_thread_id = TRUE;
    identity.call_trace_id = 0xAABBCCDDEEFF0011ULL;
    identity.has_call_trace_id = TRUE;

    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_PROCESS) != 3U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_THREAD) != 3U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS) != 1U ||
        !has_process_call_direction(&graph, 100U, 200U) ||
        has_process_call_direction(&graph, 999U, 200U) ||
        has_process_call_direction(&graph, 100U, 999U) ||
        !has_thread_call_direction(
            &graph,
            COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL,
            100U,
            10U,
            identity.call_trace_id
        ) ||
        !has_thread_call_direction(
            &graph,
            COMTRACE_GRAPH_EDGE_CALL_TARGETS_THREAD,
            200U,
            20U,
            identity.call_trace_id
        )) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_interface_without_object_id(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    identity.has_iid = TRUE;
    identity.iid = interface_id;
    identity.has_ipid = TRUE;
    identity.ipid = ipid;

    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_COM_OBJECT) != 0U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_IID) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_IPID) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID) != 1U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_oid_without_known_process(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    identity.process_id = 300U;
    identity.has_process_id = FALSE;
    identity.has_oid = TRUE;
    identity.oid = 0x1122334455667788ULL;

    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_COM_OBJECT) != 0U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_PROCESS) != 0U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_related_activity_without_activity(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    const COMTRACE_GRAPH_NODE *node;
    size_t index;
    int found = 0;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    identity.process_id = 300U;
    identity.thread_id = 30U;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    activity_ids.has_related_activity_id = TRUE;
    activity_ids.related_activity_id = related_activity_id;

    if (comtrace_relationship_graph_add_event(
            &graph,
            &identity,
            &activity_ids
        ) != COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_ACTIVITY) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_THREAD_HAS_ACTIVITY) != 0U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO) != 0U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        node = comtrace_relationship_graph_get_node(&graph, index);
        if (node != NULL &&
            node->type == COMTRACE_GRAPH_NODE_ACTIVITY &&
            memcmp(&node->guid, &related_activity_id, sizeof(GUID)) == 0) {
            found = 1;
            break;
        }
    }

    comtrace_relationship_graph_destroy(&graph);
    return found;
}

static int test_identical_activity_ids_do_not_create_self_edge(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    activity_ids.activity_id = activity_id;
    activity_ids.related_activity_id = activity_id;
    activity_ids.has_activity_id = TRUE;
    activity_ids.has_related_activity_id = TRUE;

    if (comtrace_relationship_graph_add_event(&graph, &identity, &activity_ids) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_ACTIVITY) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO) != 0U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_duplicate_event_does_not_duplicate_nodes_or_edges(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    size_t first_node_count;
    size_t first_edge_count;
    size_t index;
    size_t other;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    identity.process_id = 301U;
    identity.thread_id = 31U;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.target_process_id = 302U;
    identity.target_thread_id = 32U;
    identity.has_target_process_id = TRUE;
    identity.has_target_thread_id = TRUE;
    identity.call_trace_id = 0x123456789ABCDEF0ULL;
    identity.has_call_trace_id = TRUE;
    identity.oid = 0x1122334455667788ULL;
    identity.has_oid = TRUE;
    identity.clsid = class_id;
    identity.has_clsid = TRUE;
    identity.iid = interface_id;
    identity.has_iid = TRUE;
    identity.ipid = ipid;
    identity.has_ipid = TRUE;

    activity_ids.activity_id = activity_id;
    activity_ids.has_activity_id = TRUE;
    activity_ids.related_activity_id = related_activity_id;
    activity_ids.has_related_activity_id = TRUE;

    if (comtrace_relationship_graph_add_event(
            &graph,
            &identity,
            &activity_ids
        ) != COMTRACE_GRAPH_SUCCESS) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    first_node_count = graph.node_count;
    first_edge_count = graph.edge_count;

    if (comtrace_relationship_graph_add_event(
            &graph,
            &identity,
            &activity_ids
        ) != COMTRACE_GRAPH_SUCCESS ||
        graph.node_count != first_node_count ||
        graph.edge_count != first_edge_count) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *edge =
            comtrace_relationship_graph_get_edge(&graph, index);

        if (edge == NULL ||
            comtrace_relationship_graph_get_node(&graph, edge->source_index) == NULL ||
            comtrace_relationship_graph_get_node(&graph, edge->target_index) == NULL) {
            comtrace_relationship_graph_destroy(&graph);
            return 0;
        }

        for (other = index + 1U; other < graph.edge_count; ++other) {
            const COMTRACE_GRAPH_EDGE *next =
                comtrace_relationship_graph_get_edge(&graph, other);

            if (next == NULL ||
                (edge->type == next->type &&
                 edge->source_index == next->source_index &&
                 edge->target_index == next->target_index)) {
                comtrace_relationship_graph_destroy(&graph);
                return 0;
            }
        }
    }

    if (comtrace_relationship_graph_get_node(&graph, graph.node_count) != NULL ||
        comtrace_relationship_graph_get_edge(&graph, graph.edge_count) != NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_object_and_ipid_observation_windows(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    const COMTRACE_GRAPH_NODE *object_node = NULL;
    const COMTRACE_GRAPH_NODE *ipid_node = NULL;
    size_t index;
    size_t object_count = 0U;
    size_t ipid_count = 0U;
    const COMTRACE_GRAPH_NODE *second_ipid_node = NULL;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));

    identity.process_id = 400U;
    identity.has_process_id = TRUE;
    identity.has_oid = TRUE;
    identity.oid = 0x1122334455667788ULL;
    identity.has_ipid = TRUE;
    identity.ipid = ipid;
    identity.has_timestamp = TRUE;
    identity.timestamp = 9000;

    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
        COMTRACE_GRAPH_SUCCESS) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    identity.timestamp = 3000;
    identity.ipid = second_ipid;
    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
        COMTRACE_GRAPH_SUCCESS) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    identity.process_id = 401U;
    identity.timestamp = 6000;
    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
        COMTRACE_GRAPH_SUCCESS) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    identity.process_id = 400U;
    identity.oid = 0x8877665544332211ULL;
    identity.ipid = ipid;
    identity.timestamp = 12000;
    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
        COMTRACE_GRAPH_SUCCESS) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node =
            comtrace_relationship_graph_get_node(&graph, index);

        if (node == NULL) {
            comtrace_relationship_graph_destroy(&graph);
            return 0;
        }

        if (node->type == COMTRACE_GRAPH_NODE_COM_OBJECT) {
            ++object_count;
            if (node->process_id == 400U &&
                node->value == 0x1122334455667788ULL) {
                object_node = node;
            }
        } else if (node->type == COMTRACE_GRAPH_NODE_IPID) {
            ++ipid_count;
            if (memcmp(&node->guid, &ipid, sizeof(GUID)) == 0) {
                ipid_node = node;
            } else if (memcmp(&node->guid, &second_ipid, sizeof(GUID)) == 0) {
                second_ipid_node = node;
            }
        }
    }

    if (object_count != 3U ||
        ipid_count != 2U ||
        object_node == NULL ||
        object_node->observation_count != 2U ||
        !object_node->has_observed_timestamp ||
        object_node->first_seen_timestamp != 3000 ||
        object_node->last_seen_timestamp != 9000 ||
        ipid_node == NULL ||
        ipid_node->observation_count != 2U ||
        !ipid_node->has_observed_timestamp ||
        ipid_node->first_seen_timestamp != 9000 ||
        ipid_node->last_seen_timestamp != 12000 ||
        second_ipid_node == NULL ||
        second_ipid_node->observation_count != 2U ||
        !second_ipid_node->has_observed_timestamp ||
        second_ipid_node->first_seen_timestamp != 3000 ||
        second_ipid_node->last_seen_timestamp != 6000) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_observations_without_timestamps_are_counted(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    const COMTRACE_GRAPH_NODE *object_node = NULL;
    size_t index;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));

    identity.process_id = 402U;
    identity.has_process_id = TRUE;
    identity.has_oid = TRUE;
    identity.oid = 0xABCDEF0123456789ULL;

    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
        COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
        COMTRACE_GRAPH_SUCCESS) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node =
            comtrace_relationship_graph_get_node(&graph, index);

        if (node != NULL &&
            node->type == COMTRACE_GRAPH_NODE_COM_OBJECT) {
            object_node = node;
            break;
        }
    }

    if (object_node == NULL ||
        object_node->observation_count != 2U ||
        object_node->has_observed_timestamp) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_zero_process_ids_are_ignored(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));

    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.has_target_process_id = TRUE;
    identity.has_target_thread_id = TRUE;
    identity.has_oid = TRUE;
    identity.oid = 0x1122334455667788ULL;
    identity.has_call_trace_id = TRUE;
    identity.call_trace_id = 0x123456789ABCDEF0ULL;

    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_PROCESS) != 0U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_THREAD) != 0U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_COM_OBJECT) != 0U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_CALL) != 1U ||
        graph.edge_count != 0U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_zero_target_process_uses_local_process(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    const COMTRACE_GRAPH_NODE *node;
    size_t index;
    int found = 0;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));

    identity.process_id = 303U;
    identity.has_process_id = TRUE;
    identity.target_process_id = 0U;
    identity.has_target_process_id = TRUE;
    identity.oid = 0x8877665544332211ULL;
    identity.has_oid = TRUE;

    if (comtrace_relationship_graph_add_event(&graph, &identity, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_PROCESS) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_COM_OBJECT) != 1U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        node = comtrace_relationship_graph_get_node(&graph, index);
        if (node != NULL &&
            node->type == COMTRACE_GRAPH_NODE_COM_OBJECT &&
            node->process_id == 303U &&
            node->value == identity.oid) {
            found = 1;
            break;
        }
    }

    comtrace_relationship_graph_destroy(&graph);
    return found;
}

static int test_missing_relationship_fields(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));

    if (comtrace_relationship_graph_add_event(
            &graph,
            &identity,
            NULL
        ) != COMTRACE_GRAPH_SUCCESS ||
        graph.node_count != 0U ||
        graph.edge_count != 0U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    if (comtrace_relationship_graph_add_event(&graph, NULL, NULL) !=
        COMTRACE_GRAPH_INVALID_ARGUMENT) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}


static int add_process_lifetime(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    DWORD process_id,
    LONGLONG start_timestamp,
    LONGLONG end_timestamp
)
{
    COMTRACE_PROCESS_EVENT event;

    ZeroMemory(&event, sizeof(event));
    event.process_id = process_id;
    event.type = COMTRACE_PROCESS_EVENT_START;
    event.timestamp = start_timestamp;
    if (comtrace_process_lifecycle_catalog_add(catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
        return 0;
    }

    event.type = COMTRACE_PROCESS_EVENT_END;
    event.timestamp = end_timestamp;
    return comtrace_process_lifecycle_catalog_add(catalog, &event) ==
           COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
}

static int test_process_generations_separate_graph_objects(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_PROCESS_LIFECYCLE_CATALOG catalog;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    const COMTRACE_GRAPH_NODE *first_object = NULL;
    const COMTRACE_GRAPH_NODE *second_object = NULL;
    const COMTRACE_GRAPH_NODE *unknown_object = NULL;
    size_t process_count = 0U;
    size_t thread_count = 0U;
    size_t object_count = 0U;
    size_t index;

    comtrace_relationship_graph_init(&graph);
    comtrace_process_lifecycle_catalog_init(&catalog);

    if (!add_process_lifetime(&catalog, 321U, 100, 200) ||
        !add_process_lifetime(&catalog, 321U, 300, 400)) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    ZeroMemory(&identity, sizeof(identity));
    identity.process_id = 321U;
    identity.thread_id = 8U;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.has_oid = TRUE;
    identity.oid = 0x1122334455667788ULL;
    identity.has_timestamp = TRUE;
    identity.timestamp = 150;

    if (comtrace_relationship_graph_add_event_with_lifecycle(
            &graph, &identity, NULL, &catalog
        ) != COMTRACE_GRAPH_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    identity.timestamp = 160;
    if (comtrace_relationship_graph_add_event_with_lifecycle(
            &graph, &identity, NULL, &catalog
        ) != COMTRACE_GRAPH_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    identity.timestamp = 350;
    if (comtrace_relationship_graph_add_event_with_lifecycle(
            &graph, &identity, NULL, &catalog
        ) != COMTRACE_GRAPH_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    identity.timestamp = 250;
    if (comtrace_relationship_graph_add_event_with_lifecycle(
            &graph, &identity, NULL, &catalog
        ) != COMTRACE_GRAPH_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node = &graph.nodes[index];

        if (node->type == COMTRACE_GRAPH_NODE_PROCESS) {
            ++process_count;
        } else if (node->type == COMTRACE_GRAPH_NODE_THREAD) {
            ++thread_count;
        } else if (node->type == COMTRACE_GRAPH_NODE_COM_OBJECT) {
            ++object_count;

            if (node->has_process_generation &&
                node->process_generation == 1ULL) {
                first_object = node;
            } else if (node->has_process_generation &&
                       node->process_generation == 2ULL) {
                second_object = node;
            } else if (!node->has_process_generation) {
                unknown_object = node;
            }
        }
    }

    if (process_count != 3U ||
        thread_count != 3U ||
        object_count != 3U ||
        comtrace_relationship_graph_edge_count(
            &graph,
            COMTRACE_GRAPH_EDGE_PROCESS_OWNS_OBJECT
        ) != 3U ||
        first_object == NULL ||
        first_object->process_id != 321U ||
        first_object->value != 0x1122334455667788ULL ||
        first_object->observation_count != 2ULL ||
        second_object == NULL ||
        second_object->process_id != 321U ||
        second_object->value != 0x1122334455667788ULL ||
        second_object->observation_count != 1ULL ||
        unknown_object == NULL ||
        unknown_object->process_id != 321U ||
        unknown_object->observation_count != 1ULL) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_process_lifecycle_catalog_destroy(&catalog);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_object_owner_uses_target_process_generation(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_PROCESS_LIFECYCLE_CATALOG catalog;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    const COMTRACE_GRAPH_NODE *object_node = NULL;
    const COMTRACE_GRAPH_NODE *owner_node = NULL;
    size_t index;

    comtrace_relationship_graph_init(&graph);
    comtrace_process_lifecycle_catalog_init(&catalog);

    if (!add_process_lifetime(&catalog, 410U, 100, 200) ||
        !add_process_lifetime(&catalog, 410U, 300, 400)) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    ZeroMemory(&identity, sizeof(identity));
    identity.process_id = 99U;
    identity.has_process_id = TRUE;
    identity.target_process_id = 410U;
    identity.has_target_process_id = TRUE;
    identity.has_oid = TRUE;
    identity.oid = 0x8877665544332211ULL;
    identity.has_timestamp = TRUE;
    identity.timestamp = 350;

    if (comtrace_relationship_graph_add_event_with_lifecycle(
            &graph, &identity, NULL, &catalog
        ) != COMTRACE_GRAPH_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node = &graph.nodes[index];

        if (node->type == COMTRACE_GRAPH_NODE_COM_OBJECT) {
            object_node = node;
        } else if (node->type == COMTRACE_GRAPH_NODE_PROCESS &&
                   node->process_id == 410U) {
            owner_node = node;
        }
    }

    if (object_node == NULL ||
        object_node->process_id != 410U ||
        !object_node->has_process_generation ||
        object_node->process_generation != 2ULL ||
        owner_node == NULL ||
        !owner_node->has_process_generation ||
        owner_node->process_generation != 2ULL ||
        comtrace_relationship_graph_edge_count(
            &graph,
            COMTRACE_GRAPH_EDGE_PROCESS_OWNS_OBJECT
        ) != 1U) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_process_lifecycle_catalog_destroy(&catalog);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}


static int test_instrumented_lifetime_object_graph(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_LIFETIME_RECORD record;
    const COMTRACE_GRAPH_NODE *node;
    size_t index;
    size_t lifetime_node_index = (size_t)-1;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&record, sizeof(record));

    record.process_id = 400U;
    record.process_generation = 2ULL;
    record.has_process_generation = TRUE;
    record.object_instance_id = 0x1234ULL;
    record.has_created = TRUE;
    record.created_timestamp = 100ULL;
    record.has_destroyed = TRUE;
    record.destroyed_timestamp = 250ULL;
    record.has_class_id = TRUE;
    record.class_id = class_id;
    record.has_interface_id = TRUE;
    record.interface_id = interface_id;
    record.observation_count = 4ULL;
    record.first_seen_timestamp = 100ULL;
    record.last_seen_timestamp = 250ULL;

    if (comtrace_relationship_graph_add_lifetime_record(&graph, &record) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_lifetime_record(&graph, &record) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(
            &graph,
            COMTRACE_GRAPH_NODE_COM_LIFETIME_OBJECT
        ) != 1U ||
        comtrace_relationship_graph_edge_count(
            &graph,
            COMTRACE_GRAPH_EDGE_PROCESS_OWNS_LIFETIME_OBJECT
        ) != 1U ||
        comtrace_relationship_graph_edge_count(
            &graph,
            COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_HAS_CLSID
        ) != 1U ||
        comtrace_relationship_graph_edge_count(
            &graph,
            COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_EXPOSES_IID
        ) != 1U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        node = comtrace_relationship_graph_get_node(&graph, index);
        if (node != NULL &&
            node->type == COMTRACE_GRAPH_NODE_COM_LIFETIME_OBJECT) {
            lifetime_node_index = index;
            break;
        }
    }

    node = comtrace_relationship_graph_get_node(&graph, lifetime_node_index);
    if (node == NULL ||
        node->process_id != 400U ||
        node->value != 0x1234ULL ||
        !node->has_process_generation ||
        node->process_generation != 2ULL ||
        node->observation_count != 4ULL ||
        !node->has_observed_timestamp ||
        node->first_seen_timestamp != 100LL ||
        node->last_seen_timestamp != 250LL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    record.process_generation = 3ULL;
    if (comtrace_relationship_graph_add_lifetime_record(&graph, &record) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(
            &graph,
            COMTRACE_GRAPH_NODE_COM_LIFETIME_OBJECT
        ) != 2U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_multiple_lifetime_interfaces_and_native_link(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_LIFETIME_RECORD record;
    COMTRACE_LIFETIME_INTERFACE_ID interfaces[2];
    COMTRACE_LIFETIME_NATIVE_IDENTITY native_identity;
    size_t index;
    int found_native_object = 0;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&record, sizeof(record));
    ZeroMemory(interfaces, sizeof(interfaces));
    ZeroMemory(&native_identity, sizeof(native_identity));

    interfaces[0].interface_id = class_id;
    interfaces[1].interface_id = interface_id;
    native_identity.native_object_id = 0xABCDEF0123456789ULL;
    native_identity.has_native_ipid = TRUE;
    native_identity.native_ipid = interface_id;
    native_identity.has_interface_id = TRUE;
    native_identity.interface_id = interface_id;

    record.process_id = 401U;
    record.object_instance_id = 0x5678ULL;
    record.interface_ids = interfaces;
    record.interface_id_count = 2U;
    record.native_identities = &native_identity;
    record.native_identity_count = 1U;

    if (comtrace_relationship_graph_add_lifetime_record(&graph, &record) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_IID) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_COM_OBJECT) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_EXPOSES_IID) != 2U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_MAPS_TO_NATIVE_OBJECT) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID) != 1U) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node = &graph.nodes[index];
        if (node->type == COMTRACE_GRAPH_NODE_COM_OBJECT &&
            node->process_id == 401U &&
            node->value == 0xABCDEF0123456789ULL) {
            found_native_object = 1;
        }
    }

    comtrace_relationship_graph_destroy(&graph);
    return found_native_object;
}

int main(void)
{
    if (!test_client_server_graph() ||
        !test_explicit_source_and_target_processes() ||
        !test_interface_without_object_id() ||
        !test_oid_without_known_process() ||
        !test_related_activity_without_activity() ||
        !test_identical_activity_ids_do_not_create_self_edge() ||
        !test_duplicate_event_does_not_duplicate_nodes_or_edges() ||
        !test_zero_process_ids_are_ignored() ||
        !test_zero_target_process_uses_local_process() ||
        !test_object_and_ipid_observation_windows() ||
        !test_observations_without_timestamps_are_counted() ||
        !test_process_generations_separate_graph_objects() ||
        !test_object_owner_uses_target_process_generation() ||
        !test_missing_relationship_fields() ||
        !test_instrumented_lifetime_object_graph() ||
        !test_multiple_lifetime_interfaces_and_native_link()) {
        fputs("Relationship graph tests failed.\n", stderr);
        return 1;
    }

    puts("Relationship graph tests passed.");
    return 0;
}
