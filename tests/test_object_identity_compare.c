#include <stdio.h>
#include <string.h>
#include "object_identity_compare.h"

static const GUID class_one = {
    0x00112233, 0x4455, 0x6677,
    { 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }
};

static const GUID class_two = {
    0x10112233, 0x4455, 0x6677,
    { 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }
};

static const GUID iid_one = {
    0x10213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const GUID iid_two = {
    0x20213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const GUID iid_three = {
    0x30213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const GUID ipid_one = {
    0x40516273, 0x8495, 0xa6b7,
    { 0xc8, 0xd9, 0xea, 0xfb, 0x0c, 0x1d, 0x2e, 0x3f }
};

static const GUID ipid_two = {
    0x50516273, 0x8495, 0xa6b7,
    { 0xc8, 0xd9, 0xea, 0xfb, 0x0c, 0x1d, 0x2e, 0x3f }
};

static const GUID ipid_three = {
    0x60516273, 0x8495, 0xa6b7,
    { 0xc8, 0xd9, 0xea, 0xfb, 0x0c, 0x1d, 0x2e, 0x3f }
};

static size_t find_object(const COMTRACE_RELATIONSHIP_GRAPH *graph)
{
    size_t index;

    for (index = 0U; index < graph->node_count; ++index) {
        if (graph->nodes[index].type == COMTRACE_GRAPH_NODE_COM_OBJECT) {
            return index;
        }
    }

    return (size_t)-1;
}

static int add_identity(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const GUID *clsid,
    const GUID *iid,
    const GUID *ipid
)
{
    COMTRACE_COM_OBJECT_IDENTITY identity;

    ZeroMemory(&identity, sizeof(identity));
    identity.process_id = 4242U;
    identity.has_process_id = TRUE;
    identity.has_oid = TRUE;
    identity.oid = 0x1122334455667788ULL;

    if (clsid != NULL) {
        identity.has_clsid = TRUE;
        identity.clsid = *clsid;
    }
    if (iid != NULL) {
        identity.has_iid = TRUE;
        identity.iid = *iid;
    }
    if (ipid != NULL) {
        identity.has_ipid = TRUE;
        identity.ipid = *ipid;
    }

    return comtrace_relationship_graph_add_event(graph, &identity, NULL) ==
           COMTRACE_GRAPH_SUCCESS;
}

static int build_graph(
    COMTRACE_RELATIONSHIP_GRAPH *graph,
    const GUID *clsid,
    const GUID *iid,
    const GUID *ipid
)
{
    comtrace_relationship_graph_init(graph);
    return add_identity(graph, clsid, iid, ipid);
}

static ULONG compare_graphs(
    const GUID *left_clsid,
    const GUID *left_iid,
    const GUID *left_ipid,
    const GUID *right_clsid,
    const GUID *right_iid,
    const GUID *right_ipid
)
{
    COMTRACE_RELATIONSHIP_GRAPH left_graph;
    COMTRACE_RELATIONSHIP_GRAPH right_graph;
    size_t left_index;
    size_t right_index;
    ULONG changed_fields = 0U;

    if (!build_graph(&left_graph, left_clsid, left_iid, left_ipid)) {
        comtrace_relationship_graph_destroy(&left_graph);
        return 0xFFFFFFFFUL;
    }

    if (!build_graph(&right_graph, right_clsid, right_iid, right_ipid)) {
        comtrace_relationship_graph_destroy(&right_graph);
        comtrace_relationship_graph_destroy(&left_graph);
        return 0xFFFFFFFFUL;
    }

    left_index = find_object(&left_graph);
    right_index = find_object(&right_graph);

    changed_fields = comtrace_compare_object_identity_fields(
        &left_graph,
        left_index,
        &right_graph,
        right_index
    );

    comtrace_relationship_graph_destroy(&right_graph);
    comtrace_relationship_graph_destroy(&left_graph);
    return changed_fields;
}

static ULONG compare_multiple_interfaces(
    const GUID *right_iid,
    const GUID *right_ipid
)
{
    COMTRACE_RELATIONSHIP_GRAPH left_graph;
    COMTRACE_RELATIONSHIP_GRAPH right_graph;
    ULONG changed_fields;

    if (!build_graph(&left_graph, &class_one, &iid_one, &ipid_one)) {
        comtrace_relationship_graph_destroy(&left_graph);
        return 0xFFFFFFFFUL;
    }

    if (!build_graph(&right_graph, &class_one, &iid_one, &ipid_one)) {
        comtrace_relationship_graph_destroy(&left_graph);
        comtrace_relationship_graph_destroy(&right_graph);
        return 0xFFFFFFFFUL;
    }

    if (!add_identity(&left_graph, NULL, &iid_two, &ipid_two) ||
        !add_identity(&right_graph, NULL, right_iid, right_ipid)) {
        comtrace_relationship_graph_destroy(&right_graph);
        comtrace_relationship_graph_destroy(&left_graph);
        return 0xFFFFFFFFUL;
    }

    changed_fields = comtrace_compare_object_identity_fields(
        &left_graph,
        find_object(&left_graph),
        &right_graph,
        find_object(&right_graph)
    );

    comtrace_relationship_graph_destroy(&right_graph);
    comtrace_relationship_graph_destroy(&left_graph);
    return changed_fields;
}

int main(void)
{
    ULONG changed;

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             &class_one, &iid_one, &ipid_one);
    if (changed != 0U) {
        fputs("Equal object identities were reported as changed.\n", stderr);
        return 1;
    }

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             &class_two, &iid_two, &ipid_two);
    if (changed != (COMTRACE_OBJECT_CHANGE_CLSID |
                    COMTRACE_OBJECT_CHANGE_IID |
                    COMTRACE_OBJECT_CHANGE_IPID)) {
        fputs("All changed object identity fields were not detected.\n", stderr);
        return 1;
    }

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             &class_two, &iid_one, &ipid_one);
    if (changed != COMTRACE_OBJECT_CHANGE_CLSID) {
        fputs("CLSID change detection failed.\n", stderr);
        return 1;
    }

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             &class_one, &iid_two, &ipid_one);
    if (changed != COMTRACE_OBJECT_CHANGE_IID) {
        fputs("IID change detection failed.\n", stderr);
        return 1;
    }

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             &class_one, &iid_one, &ipid_two);
    if (changed != COMTRACE_OBJECT_CHANGE_IPID) {
        fputs("IPID change detection failed.\n", stderr);
        return 1;
    }

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             NULL, &iid_one, &ipid_one);
    if (changed != COMTRACE_OBJECT_CHANGE_CLSID) {
        fputs("Missing CLSID detection failed.\n", stderr);
        return 1;
    }

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             &class_one, NULL, &ipid_one);
    if (changed != COMTRACE_OBJECT_CHANGE_IID) {
        fputs("Missing IID detection failed.\n", stderr);
        return 1;
    }

    changed = compare_graphs(&class_one, &iid_one, &ipid_one,
                             &class_one, &iid_one, NULL);
    if (changed != (COMTRACE_OBJECT_CHANGE_IID |
                    COMTRACE_OBJECT_CHANGE_IPID)) {
        fputs("Missing IPID and linked IID detection failed.\n", stderr);
        return 1;
    }

    changed = compare_multiple_interfaces(&iid_three, &ipid_three);
    if (changed != (COMTRACE_OBJECT_CHANGE_IID |
                    COMTRACE_OBJECT_CHANGE_IPID)) {
        fputs("Changes to secondary IPID and IID were not detected.\n", stderr);
        return 1;
    }

    changed = compare_multiple_interfaces(&iid_one, &ipid_one);
    if (changed != 0U) {
        fputs("Matching multiple-interface sets were reported as changed.\n", stderr);
        return 1;
    }

    puts("COM object identity comparison tests passed.");
    return 0;
}
