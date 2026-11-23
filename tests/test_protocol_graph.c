#include "relationship_graph.h"

#include <stdio.h>
#include <stdlib.h>

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
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_RPC_CALL_HAS_ACTIVITY) != 2U) {
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
    timeline.records[0].conversation_correlation_id = 50U;
    timeline.records[0].transaction_instance_id = 60U;
    timeline.records[0].transaction_correlation_id = 70U;
    timeline.records[1].process_id = 400U;
    timeline.records[1].thread_id = 401U;
    timeline.records[1].conversation_id = 41U;
    timeline.records[1].conversation_correlation_id = 50U;
    timeline.records[1].transaction_instance_id = 61U;
    timeline.records[1].transaction_correlation_id = 70U;
    timeline.exchanges[0].client_record_index = 0U;
    timeline.exchanges[0].server_record_index = 1U;

    if (comtrace_relationship_graph_add_dde_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_CONVERSATION) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_TRANSACTION) != 2U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_CONVERSATION_CORRELATION) != 1U ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_DDE_TRANSACTION_CORRELATION) != 1U ||
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
    timeline.records[0].has_start = TRUE;
    timeline.records[0].has_activity_id = TRUE;
    timeline.records[0].activity_id.Data1 = 80U;
    if (comtrace_relationship_graph_add_ole_timeline(&graph, &timeline, NULL) !=
            COMTRACE_GRAPH_SUCCESS ||
        comtrace_relationship_graph_node_count(&graph, COMTRACE_GRAPH_NODE_OLE_OPERATION) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_OLE_OPERATION_HAS_ACTIVITY) != 1U ||
        comtrace_relationship_graph_edge_count(&graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_OLE_OPERATION) != 1U) {
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
    wprintf(L"RPC, DDE and OLE relationship graph tests passed.\n");
    return 0;
}
