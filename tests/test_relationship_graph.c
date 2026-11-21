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

int main(void)
{
    if (!test_client_server_graph() ||
        !test_interface_without_object_id() ||
        !test_missing_relationship_fields()) {
        fputs("Relationship graph tests failed.\n", stderr);
        return 1;
    }

    puts("Relationship graph tests passed.");
    return 0;
}
