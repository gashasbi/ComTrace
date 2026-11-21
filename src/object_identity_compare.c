#include "object_identity_compare.h"

#include <string.h>

static int get_linked_guid(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t source_index,
    COMTRACE_GRAPH_EDGE_TYPE edge_type,
    COMTRACE_GRAPH_NODE_TYPE node_type,
    GUID *guid
)
{
    size_t index;

    for (index = 0U; index < graph->edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *edge = &graph->edges[index];
        const COMTRACE_GRAPH_NODE *node;

        if (edge->type != edge_type ||
            edge->source_index != source_index ||
            edge->target_index >= graph->node_count) {
            continue;
        }

        node = &graph->nodes[edge->target_index];
        if (node->type == node_type) {
            *guid = node->guid;
            return 1;
        }
    }

    return 0;
}

static int get_object_iid(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t object_index,
    GUID *iid
)
{
    size_t ipid_index;
    size_t index;

    for (index = 0U; index < graph->edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *object_edge = &graph->edges[index];

        if (object_edge->type != COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID ||
            object_edge->source_index != object_index ||
            object_edge->target_index >= graph->node_count) {
            continue;
        }

        ipid_index = object_edge->target_index;
        if (get_linked_guid(
                graph,
                ipid_index,
                COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID,
                COMTRACE_GRAPH_NODE_IID,
                iid)) {
            return 1;
        }
    }

    return 0;
}

ULONG comtrace_compare_object_identity_fields(
    const COMTRACE_RELATIONSHIP_GRAPH *left_graph,
    size_t left_index,
    const COMTRACE_RELATIONSHIP_GRAPH *right_graph,
    size_t right_index
)
{
    GUID left_guid;
    GUID right_guid;
    ULONG changed_fields = 0U;
    int left_has;
    int right_has;

    if (left_graph == NULL ||
        right_graph == NULL ||
        left_index >= left_graph->node_count ||
        right_index >= right_graph->node_count ||
        left_graph->nodes[left_index].type != COMTRACE_GRAPH_NODE_COM_OBJECT ||
        right_graph->nodes[right_index].type != COMTRACE_GRAPH_NODE_COM_OBJECT) {
        return 0U;
    }

    left_has = get_linked_guid(
        left_graph,
        left_index,
        COMTRACE_GRAPH_EDGE_OBJECT_HAS_CLSID,
        COMTRACE_GRAPH_NODE_CLSID,
        &left_guid
    );
    right_has = get_linked_guid(
        right_graph,
        right_index,
        COMTRACE_GRAPH_EDGE_OBJECT_HAS_CLSID,
        COMTRACE_GRAPH_NODE_CLSID,
        &right_guid
    );
    if (left_has != right_has ||
        (left_has && memcmp(&left_guid, &right_guid, sizeof(GUID)) != 0)) {
        changed_fields |= COMTRACE_OBJECT_CHANGE_CLSID;
    }

    left_has = get_linked_guid(
        left_graph,
        left_index,
        COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID,
        COMTRACE_GRAPH_NODE_IPID,
        &left_guid
    );
    right_has = get_linked_guid(
        right_graph,
        right_index,
        COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID,
        COMTRACE_GRAPH_NODE_IPID,
        &right_guid
    );
    if (left_has != right_has ||
        (left_has && memcmp(&left_guid, &right_guid, sizeof(GUID)) != 0)) {
        changed_fields |= COMTRACE_OBJECT_CHANGE_IPID;
    }

    left_has = get_object_iid(left_graph, left_index, &left_guid);
    right_has = get_object_iid(right_graph, right_index, &right_guid);
    if (left_has != right_has ||
        (left_has && memcmp(&left_guid, &right_guid, sizeof(GUID)) != 0)) {
        changed_fields |= COMTRACE_OBJECT_CHANGE_IID;
    }

    return changed_fields;
}
