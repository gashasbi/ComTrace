#include "relationship_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int test_rpc_pair(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_RPC_TIMELINE timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&timeline, sizeof(timeline));
    timeline.records = (COMTRACE_RPC_TIMELINE_RECORD *)calloc(2U, sizeof(*timeline.records));
    if (timeline.records == NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    timeline.record_count = 2U;
    timeline.records[0].process_id = 100U;
    timeline.records[0].thread_id = 101U;
    timeline.records[0].has_start = TRUE;
    timeline.records[0].has_stop = TRUE;
    timeline.records[0].stop_process_id = 102U;
    timeline.records[0].stop_thread_id = 103U;
    timeline.records[0].has_correlated_record = TRUE;
    timeline.records[0].correlated_record_index = 1U;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id.Data1 = 10U;
    timeline.records[1].process_id = 200U;
    timeline.records[1].thread_id = 201U;
    timeline.records[1].has_start = TRUE;
    timeline.records[1].has_correlated_record = TRUE;
    timeline.records[1].correlated_record_index = 0U;
    timeline.records[1].has_activity_id = TRUE;
    timeline.records[1].activity_id.Data1 = 10U;

    if (comtrace_relationship_graph_add_rpc_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_RPC_CALL) != 2U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_RPC_CALL_CORRELATES_WITH_RPC_CALL) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_RPC_CALL_HAS_ACTIVITY) != 2U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_PROCESS_COMPLETES_RPC_CALL) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_THREAD_COMPLETES_RPC_CALL) != 1U) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    free(timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_dde_exchange(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_DDE_TIMELINE timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&timeline, sizeof(timeline));
    timeline.records = (COMTRACE_DDE_TIMELINE_RECORD *)calloc(2U, sizeof(*timeline.records));
    timeline.exchanges = (COMTRACE_DDE_EXCHANGE *)calloc(1U, sizeof(*timeline.exchanges));
    if (timeline.records == NULL || timeline.exchanges == NULL) {
        free(timeline.records);
        free(timeline.exchanges);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    timeline.record_count = 2U;
    timeline.exchange_count = 1U;
    timeline.records[0].process_id = 300U;
    timeline.records[0].thread_id = 301U;
    timeline.records[0].conversation_id = 40U;
    timeline.records[0].conversation_correlation_id = 70U;
    timeline.records[0].transaction_instance_id = 60U;
    timeline.records[0].transaction_correlation_id = 70U;
    timeline.records[1].process_id = 400U;
    timeline.records[1].thread_id = 401U;
    timeline.records[1].conversation_id = 41U;
    timeline.records[1].conversation_correlation_id = 70U;
    timeline.records[1].has_callback = TRUE;
    timeline.records[1].callback_timestamp = 150U;
    timeline.records[1].transaction_correlation_id = 70U;
    timeline.exchanges[0].client_record_index = 0U;
    timeline.exchanges[0].server_record_index = 1U;

    if (comtrace_relationship_graph_add_dde_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_CONVERSATION) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_TRANSACTION) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_CALLBACK) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_CONVERSATION_CORRELATION) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_TRANSACTION_CORRELATION) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_DDE_CALLBACK_HAS_CORRELATION) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_DDE_TRANSACTION_EXCHANGES_WITH) != 1U) {
        free(timeline.records);
        free(timeline.exchanges);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    free(timeline.records);
    free(timeline.exchanges);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_ole_operation(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_OLE_TIMELINE timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&timeline, sizeof(timeline));
    timeline.records = (COMTRACE_OLE_TIMELINE_RECORD *)calloc(1U, sizeof(*timeline.records));
    if (timeline.records == NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    timeline.record_count = 1U;
    timeline.records[0].process_id = 500U;
    timeline.records[0].thread_id = 501U;
    memcpy(timeline.records[0].operation, L"OLE_DragDrop_DoDragDrop", sizeof(L"OLE_DragDrop_DoDragDrop"));
    timeline.records[0].has_start = TRUE;
    timeline.records[0].start_timestamp = 10ULL;
    timeline.records[0].has_stop = TRUE;
    timeline.records[0].stop_process_id = 502U;
    timeline.records[0].stop_thread_id = 503U;
    timeline.records[0].stop_timestamp = 20ULL;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id.Data1 = 80U;
    if (comtrace_relationship_graph_add_ole_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_OLE_OPERATION) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_OLE_OPERATION_HAS_ACTIVITY) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_OLE_OPERATION) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_PROCESS_OBSERVES_OLE_OPERATION) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_THREAD_COMPLETES_OLE_OPERATION) != 1U ||
        !graph.nodes[0].has_observed_timestamp ||
        graph.nodes[0].first_seen_timestamp != 10LL ||
        graph.nodes[0].last_seen_timestamp != 20LL ||
        wcscmp(graph.nodes[0].label, L"OLE_DragDrop_DoDragDrop") != 0) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    free(timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_ole_related_activity(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_OLE_TIMELINE timeline;
    size_t index;
    size_t operation_count = 0U;
    int found_related_only = 0;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&timeline, sizeof(timeline));
    timeline.records = (COMTRACE_OLE_TIMELINE_RECORD *)calloc(
        2U,
        sizeof(*timeline.records)
    );
    if (timeline.records == NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    timeline.record_count = 2U;
    timeline.records[0].process_id = 510U;
    timeline.records[0].thread_id = 511U;
    timeline.records[0].has_start = TRUE;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id.Data1 = 81U;
    timeline.records[0].has_related_activity_id = TRUE;
    timeline.records[0].related_activity_id.Data1 = 82U;
    memcpy(
        timeline.records[0].operation,
        L"OLE_DragDrop_DoDragDrop",
        sizeof(L"OLE_DragDrop_DoDragDrop")
    );

    timeline.records[1].process_id = 520U;
    timeline.records[1].has_start = TRUE;
    timeline.records[1].has_related_activity_id = TRUE;
    timeline.records[1].related_activity_id.Data1 = 82U;
    memcpy(
        timeline.records[1].operation,
        L"OLE_Clipboard_GetClipboard",
        sizeof(L"OLE_Clipboard_GetClipboard")
    );

    if (comtrace_relationship_graph_add_ole_timeline(
            &graph,
            &timeline,
            NULL
        ) != COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(
            &graph,
            COMTRACE_GRAPH_NODE_ACTIVITY
        ) != 2U ||
        comtrace_relationship_graph_edge_count(
            &graph,
            COMTRACE_GRAPH_EDGE_OLE_OPERATION_HAS_ACTIVITY
        ) != 1U ||
        comtrace_relationship_graph_edge_count(
            &graph,
            COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO
        ) != 1U) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    for (index = 0U; index < graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node = &graph.nodes[index];

        if (node->type == COMTRACE_GRAPH_NODE_OLE_OPERATION) {
            ++operation_count;
            if (wcscmp(node->label, L"OLE_Clipboard_GetClipboard") == 0 &&
                !node->has_guid) {
                found_related_only = 1;
            }
        }
    }

    free(timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return operation_count == 2U && found_related_only;
}

static int test_ole_data_object_link(void)
{
    static const GUID data_object_iid = {
        0x0000010E, 0x0000, 0x0000,
        {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_OLE_TIMELINE timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));
    ZeroMemory(&timeline, sizeof(timeline));
    identity.process_id = 500U;
    identity.thread_id = 501U;
    identity.oid = 0x1234ULL;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.has_oid = TRUE;
    identity.iid = data_object_iid;
    identity.has_iid = TRUE;
    identity.call_trace_id = 99ULL;
    identity.has_call_trace_id = TRUE;
    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id.Data1 = 80U;
    timeline.records = (COMTRACE_OLE_TIMELINE_RECORD *)calloc(1U, sizeof(*timeline.records));
    if (timeline.records == NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    timeline.record_count = 1U;
    timeline.records[0].process_id = 510U;
    timeline.records[0].thread_id = 511U;
    timeline.records[0].has_start = TRUE;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id = activity_ids.activity_id;

    if (comtrace_relationship_graph_add_event(&graph, &identity, &activity_ids) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_ole_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_link_ole_data_objects(&graph) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_OLE_OPERATION_USES_DATA_OBJECT) != 1U) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    free(timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_ole_data_object_link_rejects_ambiguity(void)
{
    static const GUID data_object_iid = {
        0x0000010E, 0x0000, 0x0000,
        {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_OLE_TIMELINE timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));
    ZeroMemory(&timeline, sizeof(timeline));
    identity.process_id = 500U;
    identity.thread_id = 501U;
    identity.oid = 0x1234ULL;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.has_oid = TRUE;
    identity.iid = data_object_iid;
    identity.has_iid = TRUE;
    identity.call_trace_id = 99ULL;
    identity.has_call_trace_id = TRUE;
    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id.Data1 = 80U;

    timeline.records = (COMTRACE_OLE_TIMELINE_RECORD *)calloc(
        1U, sizeof(*timeline.records));
    if (timeline.records == NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    timeline.record_count = 1U;
    timeline.records[0].process_id = 510U;
    timeline.records[0].thread_id = 511U;
    timeline.records[0].has_start = TRUE;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id = activity_ids.activity_id;

    if (comtrace_relationship_graph_add_event(&graph, &identity, &activity_ids) !=
        COMTRACE_GRAPH_SUCCESS) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    identity.oid = 0x5678ULL;
    identity.call_trace_id = 100ULL;
    if (comtrace_relationship_graph_add_event(&graph, &identity, &activity_ids) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_ole_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_link_ole_data_objects(&graph) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_COM_OBJECT) != 2U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_OLE_OPERATION_USES_DATA_OBJECT) != 0U) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    free(timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_rpc_com_call_link(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_RPC_TIMELINE timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));
    ZeroMemory(&timeline, sizeof(timeline));
    identity.process_id = 600U;
    identity.thread_id = 601U;
    identity.oid = 0x2222ULL;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.has_oid = TRUE;
    identity.iid.Data1 = 0x12345678U;
    identity.has_iid = TRUE;
    identity.call_trace_id = 101ULL;
    identity.has_call_trace_id = TRUE;
    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id.Data1 = 90U;
    timeline.records = (COMTRACE_RPC_TIMELINE_RECORD *)calloc(1U, sizeof(*timeline.records));
    if (timeline.records == NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    timeline.record_count = 1U;
    timeline.records[0].process_id = 600U;
    timeline.records[0].thread_id = 601U;
    timeline.records[0].has_start = TRUE;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id = activity_ids.activity_id;
    timeline.records[0].has_interface_uuid = TRUE;
    timeline.records[0].interface_uuid = identity.iid;

    if (comtrace_relationship_graph_add_event(&graph, &identity, &activity_ids) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_rpc_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_link_rpc_com_calls(&graph) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_RPC_CALL_CORRELATES_WITH_COM_CALL) != 1U) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    free(timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_ambiguous_cross_layer_links_are_not_created(void)
{
    static const GUID data_object_iid = {
        0x0000010E, 0x0000, 0x0000,
        {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_OLE_TIMELINE ole_timeline;
    COMTRACE_RPC_TIMELINE rpc_timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(&identity, sizeof(identity));
    ZeroMemory(&activity_ids, sizeof(activity_ids));
    ZeroMemory(&ole_timeline, sizeof(ole_timeline));
    ZeroMemory(&rpc_timeline, sizeof(rpc_timeline));
    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id.Data1 = 120U;
    identity.process_id = 700U;
    identity.thread_id = 701U;
    identity.has_process_id = TRUE;
    identity.has_thread_id = TRUE;
    identity.has_oid = TRUE;
    identity.has_iid = TRUE;
    identity.iid = data_object_iid;
    identity.has_call_trace_id = TRUE;
    identity.oid = 0x3001ULL;
    identity.call_trace_id = 301ULL;

    ole_timeline.records = (COMTRACE_OLE_TIMELINE_RECORD *)calloc(1U, sizeof(*ole_timeline.records));
    rpc_timeline.records = (COMTRACE_RPC_TIMELINE_RECORD *)calloc(1U, sizeof(*rpc_timeline.records));
    if (ole_timeline.records == NULL || rpc_timeline.records == NULL) {
        free(ole_timeline.records);
        free(rpc_timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    ole_timeline.record_count = 1U;
    ole_timeline.records[0].process_id = 700U;
    ole_timeline.records[0].has_start = TRUE;
    ole_timeline.records[0].has_activity_id = TRUE;
    ole_timeline.records[0].activity_id = activity_ids.activity_id;
    rpc_timeline.record_count = 1U;
    rpc_timeline.records[0].process_id = 700U;
    rpc_timeline.records[0].has_start = TRUE;
    rpc_timeline.records[0].has_activity_id = TRUE;
    rpc_timeline.records[0].activity_id = activity_ids.activity_id;
    rpc_timeline.records[0].has_interface_uuid = TRUE;
    rpc_timeline.records[0].interface_uuid = data_object_iid;

    if (comtrace_relationship_graph_add_event(&graph, &identity, &activity_ids) !=
            COMTRACE_GRAPH_SUCCESS) {
        free(ole_timeline.records);
        free(rpc_timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    identity.oid = 0x3002ULL;
    identity.call_trace_id = 302ULL;
    if (comtrace_relationship_graph_add_event(&graph, &identity, &activity_ids) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_ole_timeline(&graph, &ole_timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_rpc_timeline(&graph, &rpc_timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_link_ole_data_objects(&graph) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_link_rpc_com_calls(&graph) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_OLE_OPERATION_USES_DATA_OBJECT) != 0U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_RPC_CALL_CORRELATES_WITH_COM_CALL) != 0U) {
        free(ole_timeline.records);
        free(rpc_timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    free(ole_timeline.records);
    free(rpc_timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

static int test_rpc_request_response_chain(void)
{
    COMTRACE_RELATIONSHIP_GRAPH graph;
    COMTRACE_COM_OBJECT_IDENTITY identities[2];
    COMTRACE_ACTIVITY_IDS activities[2];
    COMTRACE_RPC_TIMELINE timeline;

    comtrace_relationship_graph_init(&graph);
    ZeroMemory(identities, sizeof(identities));
    ZeroMemory(activities, sizeof(activities));
    ZeroMemory(&timeline, sizeof(timeline));
    timeline.records = (COMTRACE_RPC_TIMELINE_RECORD *)calloc(2U, sizeof(*timeline.records));
    if (timeline.records == NULL) {
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    activities[0].has_activity_id = TRUE;
    activities[0].activity_id.Data1 = 130U;
    activities[0].has_related_activity_id = TRUE;
    activities[0].related_activity_id.Data1 = 131U;
    activities[1].has_activity_id = TRUE;
    activities[1].activity_id.Data1 = 131U;
    activities[1].has_related_activity_id = TRUE;
    activities[1].related_activity_id.Data1 = 130U;

    identities[0].process_id = 610U;
    identities[0].thread_id = 611U;
    identities[0].oid = 0x4101ULL;
    identities[0].has_process_id = TRUE;
    identities[0].has_thread_id = TRUE;
    identities[0].has_oid = TRUE;
    identities[0].iid.Data1 = 0x12345678U;
    identities[0].has_iid = TRUE;
    identities[0].call_trace_id = 4101ULL;
    identities[0].has_call_trace_id = TRUE;
    identities[1] = identities[0];
    identities[1].process_id = 620U;
    identities[1].thread_id = 621U;
    identities[1].oid = 0x4201ULL;
    identities[1].call_trace_id = 4201ULL;

    if (comtrace_relationship_graph_add_event(&graph, &identities[0], &activities[0]) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_add_event(&graph, &identities[1], &activities[1]) !=
            COMTRACE_GRAPH_SUCCESS) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }

    timeline.record_count = 2U;
    timeline.records[0].process_id = 610U;
    timeline.records[0].thread_id = 611U;
    timeline.records[0].has_start = TRUE;
    timeline.records[0].role = COMTRACE_RPC_ROLE_CLIENT;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id = activities[0].activity_id;
    timeline.records[0].has_related_activity_id = TRUE;
    timeline.records[0].related_activity_id = activities[0].related_activity_id;
    timeline.records[0].has_interface_uuid = TRUE;
    timeline.records[0].interface_uuid = identities[0].iid;
    timeline.records[0].has_correlated_record = TRUE;
    timeline.records[0].correlated_record_index = 1U;

    timeline.records[1].process_id = 620U;
    timeline.records[1].thread_id = 621U;
    timeline.records[1].has_start = TRUE;
    timeline.records[1].role = COMTRACE_RPC_ROLE_SERVER;
    timeline.records[1].has_activity_id = TRUE;
    timeline.records[1].activity_id = activities[1].activity_id;
    timeline.records[1].has_related_activity_id = TRUE;
    timeline.records[1].related_activity_id = activities[1].related_activity_id;
    timeline.records[1].has_interface_uuid = TRUE;
    timeline.records[1].interface_uuid = identities[1].iid;
    timeline.records[1].has_correlated_record = TRUE;
    timeline.records[1].correlated_record_index = 0U;

    if (comtrace_relationship_graph_add_rpc_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_link_rpc_com_calls(&graph) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_RPC_CALL_CORRELATES_WITH_RPC_CALL) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_RPC_CALL_CORRELATES_WITH_COM_CALL) != 2U) {
        free(timeline.records);
        comtrace_relationship_graph_destroy(&graph);
        return 0;
    }
    free(timeline.records);
    comtrace_relationship_graph_destroy(&graph);
    return 1;
}

int main(void)
{
    if (!test_rpc_pair()) {
        fwprintf(stderr, L"RPC relationship graph test failed.\n");
        return 1;
    }
    if (!test_dde_exchange()) {
        fwprintf(stderr, L"DDE relationship graph test failed.\n");
        return 1;
    }
    if (!test_ole_operation()) {
        fwprintf(stderr, L"OLE relationship graph test failed.\n");
        return 1;
    }
    if (!test_ole_related_activity()) {
        fwprintf(stderr, L"OLE related activity graph test failed.\n");
        return 1;
    }
    if (!test_ole_data_object_link()) {
        fwprintf(stderr, L"OLE DataObject graph link test failed.\n");
        return 1;
    }
    if (!test_ole_data_object_link_rejects_ambiguity()) {
        fwprintf(stderr, L"Ambiguous OLE DataObject graph test failed.\n");
        return 1;
    }
    if (!test_rpc_com_call_link()) {
        fwprintf(stderr, L"RPC to COM graph link test failed.\n");
        return 1;
    }
    if (!test_ambiguous_cross_layer_links_are_not_created()) {
        fwprintf(stderr, L"Ambiguous graph links test failed.\n");
        return 1;
    }
    if (!test_rpc_request_response_chain()) {
        fwprintf(stderr, L"RPC request/response graph chain test failed.\n");
        return 1;
    }
    wprintf(L"RPC, DDE and OLE relationship graph tests passed.\n");
    return 0;
}
