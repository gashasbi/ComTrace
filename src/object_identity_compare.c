#include "object_identity_compare.h"

#include <string.h>

static int object_has_guid(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t object_index,
    COMTRACE_GRAPH_EDGE_TYPE edge_type,
    COMTRACE_GRAPH_NODE_TYPE node_type,
    const GUID *guid
)
{
    size_t index;

    for (index = 0U; index < graph->edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *edge = &graph->edges[index];
        const COMTRACE_GRAPH_NODE *node;

        if (edge->type != edge_type ||
            edge->source_index != object_index ||
            edge->target_index >= graph->node_count) {
            continue;
        }

        node = &graph->nodes[edge->target_index];
        if (node->type == node_type &&
            memcmp(&node->guid, guid, sizeof(GUID)) == 0) {
            return 1;
        }
    }

    return 0;
}

static int object_has_iid(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    size_t object_index,
    const GUID *iid
)
{
    size_t object_edge_index;
    size_t iid_edge_index;

    for (object_edge_index = 0U;
         object_edge_index < graph->edge_count;
         ++object_edge_index) {
        const COMTRACE_GRAPH_EDGE *object_edge =
            &graph->edges[object_edge_index];
        size_t ipid_index;

        if (object_edge->type != COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID ||
            object_edge->source_index != object_index ||
            object_edge->target_index >= graph->node_count) {
            continue;
        }

        ipid_index = object_edge->target_index;
        for (iid_edge_index = 0U;
             iid_edge_index < graph->edge_count;
             ++iid_edge_index) {
            const COMTRACE_GRAPH_EDGE *iid_edge =
                &graph->edges[iid_edge_index];
            const COMTRACE_GRAPH_NODE *iid_node;

            if (iid_edge->type != COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID ||
                iid_edge->source_index != ipid_index ||
                iid_edge->target_index >= graph->node_count) {
                continue;
            }

            iid_node = &graph->nodes[iid_edge->target_index];
            if (iid_node->type == COMTRACE_GRAPH_NODE_IID &&
                memcmp(&iid_node->guid, iid, sizeof(GUID)) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int object_guid_sets_differ(
    const COMTRACE_RELATIONSHIP_GRAPH *left_graph,
    size_t left_index,
    const COMTRACE_RELATIONSHIP_GRAPH *right_graph,
    size_t right_index,
    COMTRACE_GRAPH_EDGE_TYPE edge_type,
    COMTRACE_GRAPH_NODE_TYPE node_type
)
{
    size_t index;

    for (index = 0U; index < left_graph->edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *edge = &left_graph->edges[index];
        const COMTRACE_GRAPH_NODE *node;

        if (edge->type != edge_type ||
            edge->source_index != left_index ||
            edge->target_index >= left_graph->node_count) {
            continue;
        }

        node = &left_graph->nodes[edge->target_index];
        if (node->type == node_type &&
            !object_has_guid(
                right_graph,
                right_index,
                edge_type,
                node_type,
                &node->guid
            )) {
            return 1;
        }
    }

    for (index = 0U; index < right_graph->edge_count; ++index) {
        const COMTRACE_GRAPH_EDGE *edge = &right_graph->edges[index];
        const COMTRACE_GRAPH_NODE *node;

        if (edge->type != edge_type ||
            edge->source_index != right_index ||
            edge->target_index >= right_graph->node_count) {
            continue;
        }

        node = &right_graph->nodes[edge->target_index];
        if (node->type == node_type &&
            !object_has_guid(
                left_graph,
                left_index,
                edge_type,
                node_type,
                &node->guid
            )) {
            return 1;
        }
    }

    return 0;
}

static int object_iid_sets_differ(
    const COMTRACE_RELATIONSHIP_GRAPH *left_graph,
    size_t left_index,
    const COMTRACE_RELATIONSHIP_GRAPH *right_graph,
    size_t right_index
)
{
    size_t object_edge_index;
    size_t iid_edge_index;

    for (object_edge_index = 0U;
         object_edge_index < left_graph->edge_count;
         ++object_edge_index) {
        const COMTRACE_GRAPH_EDGE *object_edge =
            &left_graph->edges[object_edge_index];
        size_t ipid_index;

        if (object_edge->type != COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID ||
            object_edge->source_index != left_index ||
            object_edge->target_index >= left_graph->node_count) {
            continue;
        }

        ipid_index = object_edge->target_index;
        for (iid_edge_index = 0U;
             iid_edge_index < left_graph->edge_count;
             ++iid_edge_index) {
            const COMTRACE_GRAPH_EDGE *iid_edge =
                &left_graph->edges[iid_edge_index];
            const COMTRACE_GRAPH_NODE *iid_node;

            if (iid_edge->type != COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID ||
                iid_edge->source_index != ipid_index ||
                iid_edge->target_index >= left_graph->node_count) {
                continue;
            }

            iid_node = &left_graph->nodes[iid_edge->target_index];
            if (iid_node->type == COMTRACE_GRAPH_NODE_IID &&
                !object_has_iid(right_graph, right_index, &iid_node->guid)) {
                return 1;
            }
        }
    }

    for (object_edge_index = 0U;
         object_edge_index < right_graph->edge_count;
         ++object_edge_index) {
        const COMTRACE_GRAPH_EDGE *object_edge =
            &right_graph->edges[object_edge_index];
        size_t ipid_index;

        if (object_edge->type != COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID ||
            object_edge->source_index != right_index ||
            object_edge->target_index >= right_graph->node_count) {
            continue;
        }

        ipid_index = object_edge->target_index;
        for (iid_edge_index = 0U;
             iid_edge_index < right_graph->edge_count;
             ++iid_edge_index) {
            const COMTRACE_GRAPH_EDGE *iid_edge =
                &right_graph->edges[iid_edge_index];
            const COMTRACE_GRAPH_NODE *iid_node;

            if (iid_edge->type != COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID ||
                iid_edge->source_index != ipid_index ||
                iid_edge->target_index >= right_graph->node_count) {
                continue;
            }

            iid_node = &right_graph->nodes[iid_edge->target_index];
            if (iid_node->type == COMTRACE_GRAPH_NODE_IID &&
                !object_has_iid(left_graph, left_index, &iid_node->guid)) {
                return 1;
            }
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
    ULONG changed_fields = 0U;

    if (left_graph == NULL ||
        right_graph == NULL ||
        left_index >= left_graph->node_count ||
        right_index >= right_graph->node_count ||
        left_graph->nodes[left_index].type != COMTRACE_GRAPH_NODE_COM_OBJECT ||
        right_graph->nodes[right_index].type != COMTRACE_GRAPH_NODE_COM_OBJECT) {
        return 0U;
    }

    if (object_guid_sets_differ(
            left_graph,
            left_index,
            right_graph,
            right_index,
            COMTRACE_GRAPH_EDGE_OBJECT_HAS_CLSID,
            COMTRACE_GRAPH_NODE_CLSID)) {
        changed_fields |= COMTRACE_OBJECT_CHANGE_CLSID;
    }

    if (object_guid_sets_differ(
            left_graph,
            left_index,
            right_graph,
            right_index,
            COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID,
            COMTRACE_GRAPH_NODE_IPID)) {
        changed_fields |= COMTRACE_OBJECT_CHANGE_IPID;
    }

    if (object_iid_sets_differ(
            left_graph,
            left_index,
            right_graph,
            right_index
        )) {
        changed_fields |= COMTRACE_OBJECT_CHANGE_IID;
    }

    return changed_fields;
}
