#include "trace_compare.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <evntrace.h>
#include <evntcons.h>

#include "archive_reader.h"
#include "com_event.h"
#include "com_identity.h"
#include "ole_event.h"

typedef struct {
    COMTRACE_TRACE_SUMMARY *summary;
    COMTRACE_TRACE_COMPARE_STATUS status;
} COMTRACE_TRACE_LOAD_CONTEXT;

static void trace_summary_init(COMTRACE_TRACE_SUMMARY *summary)
{
    ZeroMemory(summary, sizeof(*summary));
    comtrace_event_catalog_init(&summary->event_catalog);
    comtrace_relationship_graph_init(&summary->relationship_graph);
    comtrace_timeline_init(&summary->com_timeline);
    comtrace_ole_timeline_init(&summary->ole_timeline);
}

static void trace_summary_destroy(COMTRACE_TRACE_SUMMARY *summary)
{
    if (summary == NULL) {
        return;
    }

    free(summary->operation_timings);
    comtrace_ole_timeline_destroy(&summary->ole_timeline);
    comtrace_timeline_destroy(&summary->com_timeline);
    comtrace_relationship_graph_destroy(&summary->relationship_graph);
    comtrace_event_catalog_destroy(&summary->event_catalog);
    ZeroMemory(summary, sizeof(*summary));
}

static COMTRACE_TRACE_COMPARE_STATUS add_summary_event(
    COMTRACE_TRACE_SUMMARY *summary,
    const EVENT_RECORD *event_record
)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;
    const wchar_t *ole_operation = NULL;
    const wchar_t *ole_phase = NULL;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_ACTIVITY_IDS *activity_ids_ptr = NULL;
    int is_com_event;
    int is_ole_event;
    COMTRACE_EVENT_CATALOG_STATUS catalog_status;

    if (summary == NULL || event_record == NULL) {
        return COMTRACE_TRACE_COMPARE_INVALID_ARGUMENT;
    }

    if (summary->total_events == ULLONG_MAX) {
        return COMTRACE_TRACE_COMPARE_COUNT_OVERFLOW;
    }

    catalog_status = comtrace_event_catalog_add(
        &summary->event_catalog,
        event_record
    );
    if (catalog_status != COMTRACE_EVENT_CATALOG_SUCCESS) {
        return COMTRACE_TRACE_COMPARE_CATALOG_FAILED;
    }

    ++summary->total_events;

    is_com_event = comtrace_classify_com_event(
        &event_record->EventHeader.ProviderId,
        event_record->EventHeader.EventDescriptor.Id,
        event_record->EventHeader.EventDescriptor.Version,
        event_record->EventHeader.EventDescriptor.Opcode,
        &operation,
        &phase
    );

    is_ole_event = comtrace_classify_ole_event(
        &event_record->EventHeader.ProviderId,
        event_record->EventHeader.EventDescriptor.Id,
        event_record->EventHeader.EventDescriptor.Version,
        event_record->EventHeader.EventDescriptor.Opcode,
        &ole_operation,
        &ole_phase
    );

    if (!is_com_event && !is_ole_event) {
        return COMTRACE_TRACE_COMPARE_SUCCESS;
    }

    ZeroMemory(&identity, sizeof(identity));
    if (is_com_event) {
        if (comtrace_extract_com_object_identity(event_record, &identity) !=
            COMTRACE_IDENTITY_SUCCESS) {
            return COMTRACE_TRACE_COMPARE_SUCCESS;
        }
    } else {
        identity.process_id = event_record->EventHeader.ProcessId;
        identity.thread_id = event_record->EventHeader.ThreadId;
        identity.has_process_id = identity.process_id != 0U;
        identity.has_thread_id = identity.thread_id != 0U;
    }

    if (comtrace_extract_activity_ids(event_record, &activity_ids) ==
        COMTRACE_IDENTITY_SUCCESS) {
        activity_ids_ptr = &activity_ids;
    }

    if (comtrace_relationship_graph_add_event(
            &summary->relationship_graph,
            &identity,
            activity_ids_ptr
        ) != COMTRACE_GRAPH_SUCCESS) {
        return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
    }

    if (is_ole_event) {
        COMTRACE_OLE_TIMELINE_EVENT ole_event;

        if (wcscmp(ole_phase, L"Start") != 0 && wcscmp(ole_phase, L"Stop") != 0) {
            return COMTRACE_TRACE_COMPARE_SUCCESS;
        }

        ZeroMemory(&ole_event, sizeof(ole_event));
        ole_event.operation = ole_operation;
        ole_event.phase = wcscmp(ole_phase, L"Start") == 0
            ? COMTRACE_OLE_TIMELINE_PHASE_START
            : COMTRACE_OLE_TIMELINE_PHASE_STOP;
        ole_event.process_id = event_record->EventHeader.ProcessId;
        ole_event.thread_id = event_record->EventHeader.ThreadId;
        ole_event.timestamp =
            (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;
        ole_event.activity_ids = activity_ids_ptr;

        if (comtrace_ole_timeline_add_event(
                &summary->ole_timeline,
                &ole_event
            ) != COMTRACE_OLE_TIMELINE_SUCCESS) {
            return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
        }

        return COMTRACE_TRACE_COMPARE_SUCCESS;
    }

    if (wcscmp(phase, L"start") != 0 && wcscmp(phase, L"stop") != 0) {
        return COMTRACE_TRACE_COMPARE_SUCCESS;
    }

    {
        COMTRACE_TIMELINE_EVENT timeline_event;
        ULONGLONG call_trace_id = 0ULL;
        LONG hresult = 0;
        ULONG hresult_bits = 0U;

        ZeroMemory(&timeline_event, sizeof(timeline_event));
        timeline_event.operation = operation;
        timeline_event.phase = wcscmp(phase, L"start") == 0
            ? COMTRACE_TIMELINE_PHASE_START
            : COMTRACE_TIMELINE_PHASE_STOP;
        timeline_event.process_id = event_record->EventHeader.ProcessId;
        timeline_event.thread_id = event_record->EventHeader.ThreadId;
        timeline_event.timestamp =
            (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;
        timeline_event.object_identity = &identity;
        timeline_event.activity_ids = activity_ids_ptr;

        if (comtrace_extract_call_trace_id(
                event_record,
                &call_trace_id
            ) == COMTRACE_IDENTITY_SUCCESS) {
            timeline_event.has_call_trace_id = TRUE;
            timeline_event.call_trace_id = call_trace_id;
        }

        if (timeline_event.phase == COMTRACE_TIMELINE_PHASE_STOP) {
            if (comtrace_extract_int32_property(
                    event_record,
                    L"HRESULT",
                    &hresult
                ) == COMTRACE_IDENTITY_SUCCESS) {
                timeline_event.has_hresult = TRUE;
                timeline_event.hresult = (HRESULT)hresult;
            } else if (comtrace_extract_uint32_property(
                           event_record,
                           L"HRESULT",
                           &hresult_bits
                       ) == COMTRACE_IDENTITY_SUCCESS) {
                timeline_event.has_hresult = TRUE;
                timeline_event.hresult = (HRESULT)hresult_bits;
            }
        }

        if (comtrace_timeline_add_event(
                &summary->com_timeline,
                &timeline_event
            ) != COMTRACE_TIMELINE_SUCCESS) {
            return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
        }
    }

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

static VOID WINAPI trace_compare_event_callback(PEVENT_RECORD event_record)
{
    COMTRACE_TRACE_LOAD_CONTEXT *context;

    if (event_record == NULL || event_record->UserContext == NULL) {
        return;
    }

    context = (COMTRACE_TRACE_LOAD_CONTEXT *)event_record->UserContext;
    if (context->status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        return;
    }

    context->status = add_summary_event(
        context->summary,
        event_record
    );
}

static COMTRACE_TRACE_COMPARE_STATUS load_etl(
    const wchar_t *path,
    COMTRACE_TRACE_SUMMARY *summary
)
{
    EVENT_TRACE_LOGFILEW logfile;
    TRACEHANDLE trace_handle;
    ULONG process_status;
    ULONG close_status;
    COMTRACE_TRACE_LOAD_CONTEXT context;

    ZeroMemory(&logfile, sizeof(logfile));
    ZeroMemory(&context, sizeof(context));

    context.summary = summary;
    context.status = COMTRACE_TRACE_COMPARE_SUCCESS;

    logfile.LogFileName = (LPWSTR)path;
    logfile.ProcessTraceMode =
        PROCESS_TRACE_MODE_EVENT_RECORD |
        PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    logfile.EventRecordCallback = trace_compare_event_callback;
    logfile.Context = &context;

    trace_handle = OpenTraceW(&logfile);
    if (trace_handle == INVALID_PROCESSTRACE_HANDLE) {
        return COMTRACE_TRACE_COMPARE_ETL_OPEN_FAILED;
    }

    summary->trace_frequency =
        (ULONGLONG)logfile.LogfileHeader.PerfFreq.QuadPart;

    process_status = ProcessTrace(&trace_handle, 1U, NULL, NULL);
    close_status = CloseTrace(trace_handle);

    if (context.status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        return context.status;
    }

    if (process_status != ERROR_SUCCESS || close_status != ERROR_SUCCESS) {
        return COMTRACE_TRACE_COMPARE_ETL_PROCESS_FAILED;
    }

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

static COMTRACE_TRACE_COMPARE_STATUS load_ctra(
    const wchar_t *path,
    COMTRACE_TRACE_SUMMARY *summary
)
{
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    COMTRACE_ARCHIVE_READER_STATUS status;
    COMTRACE_TRACE_COMPARE_STATUS result = COMTRACE_TRACE_COMPARE_SUCCESS;

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));

    status = comtrace_archive_reader_open(&reader, path);
    if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
        comtrace_archive_reader_close(&reader);
        return COMTRACE_TRACE_COMPARE_ARCHIVE_FAILED;
    }

    for (;;) {
        status = comtrace_archive_reader_next(&reader, &event);
        if (status == COMTRACE_ARCHIVE_READER_END) {
            break;
        }

        if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
            result = COMTRACE_TRACE_COMPARE_ARCHIVE_FAILED;
            break;
        }

        result = add_summary_event(summary, &event.record);
        comtrace_archive_event_destroy(&event);
        if (result != COMTRACE_TRACE_COMPARE_SUCCESS) {
            break;
        }
    }

    if (result == COMTRACE_TRACE_COMPARE_SUCCESS) {
        summary->trace_frequency = reader.footer.perf_frequency;
    }

    comtrace_archive_event_destroy(&event);
    comtrace_archive_reader_close(&reader);
    return result;
}

static int get_trace_format(const wchar_t *path)
{
    const wchar_t *extension;

    if (path == NULL) {
        return 0;
    }

    extension = wcsrchr(path, L'.');
    if (extension == NULL) {
        return 0;
    }

    if (_wcsicmp(extension, L".etl") == 0) {
        return 1;
    }
    if (_wcsicmp(extension, L".ctra") == 0) {
        return 2;
    }

    return 0;
}

static COMTRACE_TRACE_COMPARE_STATUS load_trace(
    const wchar_t *path,
    COMTRACE_TRACE_SUMMARY *summary
)
{
    int format = get_trace_format(path);

    if (format == 1) {
        return load_etl(path, summary);
    }
    if (format == 2) {
        return load_ctra(path, summary);
    }

    return COMTRACE_TRACE_COMPARE_UNSUPPORTED_FORMAT;
}

static const COMTRACE_EVENT_KIND *find_event_kind(
    const COMTRACE_EVENT_CATALOG *catalog,
    const GUID *provider_id,
    const EVENT_DESCRIPTOR *descriptor
)
{
    size_t index;

    for (index = 0U; index < catalog->count; ++index) {
        const COMTRACE_EVENT_KIND *kind = &catalog->items[index];

        if (memcmp(&kind->provider_id, provider_id, sizeof(GUID)) == 0 &&
            kind->descriptor.Id == descriptor->Id &&
            kind->descriptor.Version == descriptor->Version &&
            kind->descriptor.Channel == descriptor->Channel &&
            kind->descriptor.Level == descriptor->Level &&
            kind->descriptor.Opcode == descriptor->Opcode &&
            kind->descriptor.Task == descriptor->Task &&
            kind->descriptor.Keyword == descriptor->Keyword) {
            return kind;
        }
    }

    return NULL;
}

static COMTRACE_TRACE_COMPARE_STATUS append_event_type_diff(
    COMTRACE_TRACE_COMPARISON *comparison,
    const GUID *provider_id,
    const EVENT_DESCRIPTOR *descriptor,
    ULONGLONG left_count,
    ULONGLONG right_count
)
{
    size_t new_count;
    COMTRACE_EVENT_TYPE_DIFF *diffs;

    if (comparison->event_type_diff_count == (size_t)-1 ||
        comparison->event_type_diff_count + 1U >
            (size_t)-1 / sizeof(*diffs)) {
        return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
    }

    new_count = comparison->event_type_diff_count + 1U;
    diffs = (COMTRACE_EVENT_TYPE_DIFF *)realloc(
        comparison->event_type_diffs,
        new_count * sizeof(*diffs)
    );
    if (diffs == NULL) {
        return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
    }

    comparison->event_type_diffs = diffs;
    diffs[comparison->event_type_diff_count].provider_id = *provider_id;
    diffs[comparison->event_type_diff_count].descriptor = *descriptor;
    diffs[comparison->event_type_diff_count].left_count = left_count;
    diffs[comparison->event_type_diff_count].right_count = right_count;
    comparison->event_type_diff_count = new_count;

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

static COMTRACE_TRACE_COMPARE_STATUS append_object_diff(
    COMTRACE_TRACE_COMPARISON *comparison,
    DWORD process_id,
    ULONGLONG object_id,
    BOOL present_in_left,
    BOOL present_in_right,
    ULONG changed_fields
)
{
    size_t new_count;
    COMTRACE_OBJECT_DIFF *diffs;

    if (comparison->object_diff_count == (size_t)-1 ||
        comparison->object_diff_count + 1U >
            (size_t)-1 / sizeof(*diffs)) {
        return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
    }

    new_count = comparison->object_diff_count + 1U;
    diffs = (COMTRACE_OBJECT_DIFF *)realloc(
        comparison->object_diffs,
        new_count * sizeof(*diffs)
    );
    if (diffs == NULL) {
        return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
    }

    comparison->object_diffs = diffs;
    diffs[comparison->object_diff_count].process_id = process_id;
    diffs[comparison->object_diff_count].object_id = object_id;
    diffs[comparison->object_diff_count].present_in_left = present_in_left;
    diffs[comparison->object_diff_count].present_in_right = present_in_right;
    diffs[comparison->object_diff_count].changed_fields = changed_fields;
    comparison->object_diff_count = new_count;

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

static size_t find_object_index(
    const COMTRACE_RELATIONSHIP_GRAPH *graph,
    DWORD process_id,
    ULONGLONG object_id
)
{
    size_t index;

    for (index = 0U; index < graph->node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node = &graph->nodes[index];

        if (node->type == COMTRACE_GRAPH_NODE_COM_OBJECT &&
            node->process_id == process_id &&
            node->value == object_id) {
            return index;
        }
    }

    return (size_t)-1;
}

static COMTRACE_TRACE_COMPARE_STATUS build_event_type_diffs(
    COMTRACE_TRACE_COMPARISON *comparison
)
{
    size_t index;

    for (index = 0U; index < comparison->left.event_catalog.count; ++index) {
        const COMTRACE_EVENT_KIND *left_kind =
            &comparison->left.event_catalog.items[index];
        const COMTRACE_EVENT_KIND *right_kind = find_event_kind(
            &comparison->right.event_catalog,
            &left_kind->provider_id,
            &left_kind->descriptor
        );

        if (right_kind == NULL) {
            ++comparison->removed_event_types;
            if (append_event_type_diff(
                    comparison,
                    &left_kind->provider_id,
                    &left_kind->descriptor,
                    left_kind->count,
                    0ULL
                ) != COMTRACE_TRACE_COMPARE_SUCCESS) {
                return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
            }
        } else if (left_kind->count != right_kind->count) {
            ++comparison->changed_event_types;
            if (append_event_type_diff(
                    comparison,
                    &left_kind->provider_id,
                    &left_kind->descriptor,
                    left_kind->count,
                    right_kind->count
                ) != COMTRACE_TRACE_COMPARE_SUCCESS) {
                return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
            }
        }
    }

    for (index = 0U; index < comparison->right.event_catalog.count; ++index) {
        const COMTRACE_EVENT_KIND *right_kind =
            &comparison->right.event_catalog.items[index];

        if (find_event_kind(
                &comparison->left.event_catalog,
                &right_kind->provider_id,
                &right_kind->descriptor) == NULL) {
            ++comparison->added_event_types;
            if (append_event_type_diff(
                    comparison,
                    &right_kind->provider_id,
                    &right_kind->descriptor,
                    0ULL,
                    right_kind->count
                ) != COMTRACE_TRACE_COMPARE_SUCCESS) {
                return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
            }
        }
    }

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

static COMTRACE_TRACE_COMPARE_STATUS build_object_diffs(
    COMTRACE_TRACE_COMPARISON *comparison
)
{
    size_t index;

    for (index = 0U; index < comparison->left.relationship_graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *left_node =
            &comparison->left.relationship_graph.nodes[index];
        size_t right_index;

        if (left_node->type != COMTRACE_GRAPH_NODE_COM_OBJECT) {
            continue;
        }

        right_index = find_object_index(
            &comparison->right.relationship_graph,
            left_node->process_id,
            left_node->value
        );

        if (right_index == (size_t)-1) {
            ++comparison->removed_objects;
            if (append_object_diff(
                    comparison,
                    left_node->process_id,
                    left_node->value,
                    TRUE,
                    FALSE,
                    0U
                ) != COMTRACE_TRACE_COMPARE_SUCCESS) {
                return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
            }
        } else {
            ULONG changed_fields = comtrace_compare_object_identity_fields(
                &comparison->left.relationship_graph,
                index,
                &comparison->right.relationship_graph,
                right_index
            );

            if (changed_fields != 0U) {
                ++comparison->changed_objects;
                if (append_object_diff(
                        comparison,
                        left_node->process_id,
                        left_node->value,
                        TRUE,
                        TRUE,
                        changed_fields
                    ) != COMTRACE_TRACE_COMPARE_SUCCESS) {
                    return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
                }
            }
        }
    }

    for (index = 0U; index < comparison->right.relationship_graph.node_count; ++index) {
        const COMTRACE_GRAPH_NODE *right_node =
            &comparison->right.relationship_graph.nodes[index];

        if (right_node->type != COMTRACE_GRAPH_NODE_COM_OBJECT) {
            continue;
        }

        if (find_object_index(
                &comparison->left.relationship_graph,
                right_node->process_id,
                right_node->value) == (size_t)-1) {
            ++comparison->added_objects;
            if (append_object_diff(
                    comparison,
                    right_node->process_id,
                    right_node->value,
                    FALSE,
                    TRUE,
                    0U
                ) != COMTRACE_TRACE_COMPARE_SUCCESS) {
                return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
            }
        }
    }

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

static COMTRACE_TRACE_COMPARE_STATUS add_operation_timing(
    COMTRACE_TRACE_SUMMARY *summary,
    const wchar_t *operation,
    ULONGLONG duration_ticks,
    BOOL has_hresult,
    HRESULT hresult
)
{
    size_t index;
    COMTRACE_OPERATION_TIMING *timing;

    for (index = 0U; index < summary->operation_timing_count; ++index) {
        if (wcscmp(
                summary->operation_timings[index].operation,
                operation) == 0) {
            timing = &summary->operation_timings[index];
            goto found;
        }
    }

    if (summary->operation_timing_count == summary->operation_timing_capacity) {
        size_t capacity = summary->operation_timing_capacity == 0U
            ? 8U
            : summary->operation_timing_capacity * 2U;
        COMTRACE_OPERATION_TIMING *timings;

        if (capacity < summary->operation_timing_capacity ||
            capacity > (size_t)-1 / sizeof(*timings)) {
            return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
        }

        timings = (COMTRACE_OPERATION_TIMING *)realloc(
            summary->operation_timings,
            capacity * sizeof(*timings)
        );
        if (timings == NULL) {
            return COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED;
        }

        summary->operation_timings = timings;
        summary->operation_timing_capacity = capacity;
    }

    timing = &summary->operation_timings[summary->operation_timing_count];
    ZeroMemory(timing, sizeof(*timing));
    if (wcslen(operation) >= _countof(timing->operation)) {
        return COMTRACE_TRACE_COMPARE_INVALID_ARGUMENT;
    }
    wcscpy_s(timing->operation, _countof(timing->operation), operation);
    ++summary->operation_timing_count;

found:
    if (timing->call_count == ULLONG_MAX ||
        (has_hresult && hresult < 0 && timing->failed_count == ULLONG_MAX)) {
        return COMTRACE_TRACE_COMPARE_COUNT_OVERFLOW;
    }

    ++timing->call_count;

    if (summary->trace_frequency != 0ULL) {
        double duration_ms =
            1000.0 * (double)duration_ticks /
            (double)summary->trace_frequency;
        timing->average_duration_ms += duration_ms;
        if (duration_ms > timing->maximum_duration_ms) {
            timing->maximum_duration_ms = duration_ms;
        }
    }

    if (has_hresult && hresult < 0) {
        ++timing->failed_count;
    }

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

static COMTRACE_TRACE_COMPARE_STATUS build_operation_timings(
    COMTRACE_TRACE_SUMMARY *summary
)
{
    size_t index;
    COMTRACE_TRACE_COMPARE_STATUS status;

    for (index = 0U; index < summary->com_timeline.record_count; ++index) {
        const COMTRACE_TIMELINE_RECORD *record =
            &summary->com_timeline.records[index];

        if (!record->has_start || !record->has_stop || !record->has_duration) {
            continue;
        }

        status = add_operation_timing(
            summary,
            record->operation,
            record->duration_ticks,
            record->has_hresult,
            record->hresult
        );
        if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
            return status;
        }
    }

    for (index = 0U; index < summary->ole_timeline.record_count; ++index) {
        const COMTRACE_OLE_TIMELINE_RECORD *record =
            &summary->ole_timeline.records[index];

        if (!record->has_start || !record->has_stop || !record->has_duration) {
            continue;
        }

        status = add_operation_timing(
            summary,
            record->operation,
            record->duration_ticks,
            FALSE,
            S_OK
        );
        if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
            return status;
        }
    }

    for (index = 0U; index < summary->operation_timing_count; ++index) {
        if (summary->operation_timings[index].call_count != 0ULL) {
            summary->operation_timings[index].average_duration_ms /=
                (double)summary->operation_timings[index].call_count;
        }
    }

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

COMTRACE_TRACE_COMPARE_STATUS comtrace_trace_compare(
    const wchar_t *left_path,
    const wchar_t *right_path,
    COMTRACE_TRACE_COMPARISON *comparison
)
{
    COMTRACE_TRACE_COMPARE_STATUS status;

    if (left_path == NULL || right_path == NULL || comparison == NULL) {
        return COMTRACE_TRACE_COMPARE_INVALID_ARGUMENT;
    }

    ZeroMemory(comparison, sizeof(*comparison));
    trace_summary_init(&comparison->left);
    trace_summary_init(&comparison->right);

    status = load_trace(left_path, &comparison->left);
    if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        comtrace_trace_comparison_destroy(comparison);
        return status;
    }

    status = load_trace(right_path, &comparison->right);
    if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        comtrace_trace_comparison_destroy(comparison);
        return status;
    }

    status = build_event_type_diffs(comparison);
    if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        comtrace_trace_comparison_destroy(comparison);
        return status;
    }

    status = build_object_diffs(comparison);
    if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        comtrace_trace_comparison_destroy(comparison);
        return status;
    }

    status = build_operation_timings(&comparison->left);
    if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        comtrace_trace_comparison_destroy(comparison);
        return status;
    }

    status = build_operation_timings(&comparison->right);
    if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        comtrace_trace_comparison_destroy(comparison);
        return status;
    }

    return COMTRACE_TRACE_COMPARE_SUCCESS;
}

void comtrace_trace_comparison_destroy(
    COMTRACE_TRACE_COMPARISON *comparison
)
{
    if (comparison == NULL) {
        return;
    }

    free(comparison->event_type_diffs);
    free(comparison->object_diffs);
    trace_summary_destroy(&comparison->left);
    trace_summary_destroy(&comparison->right);
    ZeroMemory(comparison, sizeof(*comparison));
}

static void print_guid_text(const GUID *guid)
{
    wprintf(
        L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        (unsigned long)guid->Data1,
        (unsigned int)guid->Data2,
        (unsigned int)guid->Data3,
        (unsigned int)guid->Data4[0],
        (unsigned int)guid->Data4[1],
        (unsigned int)guid->Data4[2],
        (unsigned int)guid->Data4[3],
        (unsigned int)guid->Data4[4],
        (unsigned int)guid->Data4[5],
        (unsigned int)guid->Data4[6],
        (unsigned int)guid->Data4[7]
    );
}

static const COMTRACE_OPERATION_TIMING *find_operation_timing(
    const COMTRACE_TRACE_SUMMARY *summary,
    const wchar_t *operation
)
{
    size_t index;

    for (index = 0U; index < summary->operation_timing_count; ++index) {
        if (wcscmp(summary->operation_timings[index].operation, operation) == 0) {
            return &summary->operation_timings[index];
        }
    }

    return NULL;
}

typedef struct {
    ULONGLONG matched;
    ULONGLONG matched_by_call_trace_id;
    ULONGLONG matched_by_activity_id;
    ULONGLONG matched_by_process_thread;
    ULONGLONG incomplete;
    ULONGLONG unmatched;
    ULONGLONG failed;
    ULONGLONG missing_call_trace_ids;
    ULONGLONG missing_activity_ids;
} COMTRACE_TIMELINE_COUNTS;

static COMTRACE_TIMELINE_COUNTS get_timeline_counts(
    const COMTRACE_TRACE_SUMMARY *summary
)
{
    COMTRACE_TIMELINE_COUNTS counts;
    size_t index;

    ZeroMemory(&counts, sizeof(counts));

    for (index = 0U; index < summary->com_timeline.record_count; ++index) {
        const COMTRACE_TIMELINE_RECORD *record =
            &summary->com_timeline.records[index];

        if (record->has_start && record->has_stop) {
            ++counts.matched;
            if (record->match_type == COMTRACE_TIMELINE_MATCH_CALL_TRACE_ID) {
                ++counts.matched_by_call_trace_id;
            } else if (record->match_type == COMTRACE_TIMELINE_MATCH_ACTIVITY_ID) {
                ++counts.matched_by_activity_id;
            } else if (record->match_type == COMTRACE_TIMELINE_MATCH_PROCESS_THREAD) {
                ++counts.matched_by_process_thread;
            }
        } else if (record->has_start) {
            ++counts.incomplete;
        } else if (record->has_stop) {
            ++counts.unmatched;
        }

        if (record->has_stop && record->has_hresult && record->hresult < 0) {
            ++counts.failed;
        }
    }

    counts.missing_call_trace_ids = summary->com_timeline.missing_call_trace_ids;
    counts.missing_activity_ids = summary->com_timeline.missing_activity_ids;
    return counts;
}

typedef struct {
    ULONGLONG matched;
    ULONGLONG matched_by_activity_id;
    ULONGLONG matched_by_process_thread;
    ULONGLONG incomplete;
    ULONGLONG unmatched;
    ULONGLONG activity_sequences;
    ULONGLONG missing_activity_ids;
} COMTRACE_OLE_TIMELINE_COUNTS;

static COMTRACE_OLE_TIMELINE_COUNTS get_ole_timeline_counts(
    const COMTRACE_TRACE_SUMMARY *summary
)
{
    COMTRACE_OLE_TIMELINE_COUNTS counts;
    size_t index;

    ZeroMemory(&counts, sizeof(counts));

    for (index = 0U; index < summary->ole_timeline.record_count; ++index) {
        const COMTRACE_OLE_TIMELINE_RECORD *record =
            &summary->ole_timeline.records[index];

        if (record->has_start && record->has_stop) {
            ++counts.matched;
            if (record->match_type == COMTRACE_OLE_TIMELINE_MATCH_ACTIVITY_ID) {
                ++counts.matched_by_activity_id;
            } else if (record->match_type == COMTRACE_OLE_TIMELINE_MATCH_PROCESS_THREAD) {
                ++counts.matched_by_process_thread;
            }
        } else if (record->has_start) {
            ++counts.incomplete;
        } else if (record->has_stop) {
            ++counts.unmatched;
        }
    }

    counts.activity_sequences =
        (ULONGLONG)comtrace_ole_timeline_sequence_count(&summary->ole_timeline);
    counts.missing_activity_ids = summary->ole_timeline.missing_activity_ids;
    return counts;
}

void comtrace_trace_comparison_write_text(
    const COMTRACE_TRACE_COMPARISON *comparison
)
{
    size_t index;
    COMTRACE_TIMELINE_COUNTS left_timeline;
    COMTRACE_TIMELINE_COUNTS right_timeline;
    COMTRACE_OLE_TIMELINE_COUNTS left_ole_timeline;
    COMTRACE_OLE_TIMELINE_COUNTS right_ole_timeline;

    if (comparison == NULL) {
        return;
    }

    left_timeline = get_timeline_counts(&comparison->left);
    right_timeline = get_timeline_counts(&comparison->right);
    left_ole_timeline = get_ole_timeline_counts(&comparison->left);
    right_ole_timeline = get_ole_timeline_counts(&comparison->right);

    wprintf(L"ComTrace trace comparison\n");
    wprintf(
        L"Left: events=%llu event_types=%llu COM_objects=%llu\n",
        (unsigned long long)comparison->left.total_events,
        (unsigned long long)comparison->left.event_catalog.count,
        (unsigned long long)comtrace_relationship_graph_node_count(
            &comparison->left.relationship_graph,
            COMTRACE_GRAPH_NODE_COM_OBJECT
        )
    );
    wprintf(
        L"Right: events=%llu event_types=%llu COM_objects=%llu\n",
        (unsigned long long)comparison->right.total_events,
        (unsigned long long)comparison->right.event_catalog.count,
        (unsigned long long)comtrace_relationship_graph_node_count(
            &comparison->right.relationship_graph,
            COMTRACE_GRAPH_NODE_COM_OBJECT
        )
    );
    wprintf(
        L"Event type changes: added=%llu removed=%llu count_changed=%llu\n",
        (unsigned long long)comparison->added_event_types,
        (unsigned long long)comparison->removed_event_types,
        (unsigned long long)comparison->changed_event_types
    );
    wprintf(
        L"COM object changes: added=%llu removed=%llu changed=%llu\n",
        (unsigned long long)comparison->added_objects,
        (unsigned long long)comparison->removed_objects,
        (unsigned long long)comparison->changed_objects
    );

    for (index = 0U; index < comparison->event_type_diff_count; ++index) {
        const COMTRACE_EVENT_TYPE_DIFF *diff =
            &comparison->event_type_diffs[index];

        wprintf(L"  provider=");
        print_guid_text(&diff->provider_id);
        wprintf(
            L" event_id=%u version=%u channel=%u level=%u opcode=%u task=%u keyword=0x%llx left=%llu right=%llu\n",
            (unsigned int)diff->descriptor.Id,
            (unsigned int)diff->descriptor.Version,
            (unsigned int)diff->descriptor.Channel,
            (unsigned int)diff->descriptor.Level,
            (unsigned int)diff->descriptor.Opcode,
            (unsigned int)diff->descriptor.Task,
            (unsigned long long)diff->descriptor.Keyword,
            (unsigned long long)diff->left_count,
            (unsigned long long)diff->right_count
        );   }

    for (index = 0U; index < comparison->object_diff_count; ++index) {
        const COMTRACE_OBJECT_DIFF *diff = &comparison->object_diffs[index];

        wprintf(
            L"  com_object pid=%lu oid=0x%llx left=%ls right=%ls changed_fields=0x%lx\n",
            (unsigned long)diff->process_id,
            (unsigned long long)diff->object_id,
            diff->present_in_left ? L"present" : L"absent",
            diff->present_in_right ? L"present" : L"absent",
            (unsigned long)diff->changed_fields
        );
    }

    wprintf(
        L"COM Timeline: left matched=%llu by_call_trace_id=%llu by_activity_id=%llu by_process_thread_operation=%llu incomplete=%llu unmatched=%llu missing_call_trace_id=%llu missing_activity_id=%llu failed=%llu; right matched=%llu by_call_trace_id=%llu by_activity_id=%llu by_process_thread_operation=%llu incomplete=%llu unmatched=%llu missing_call_trace_id=%llu missing_activity_id=%llu failed=%llu\n",
        (unsigned long long)left_timeline.matched,
        (unsigned long long)left_timeline.matched_by_call_trace_id,
        (unsigned long long)left_timeline.matched_by_activity_id,
        (unsigned long long)left_timeline.matched_by_process_thread,
        (unsigned long long)left_timeline.incomplete,
        (unsigned long long)left_timeline.unmatched,
        (unsigned long long)left_timeline.missing_call_trace_ids,
        (unsigned long long)left_timeline.missing_activity_ids,
        (unsigned long long)left_timeline.failed,
        (unsigned long long)right_timeline.matched,
        (unsigned long long)right_timeline.matched_by_call_trace_id,
        (unsigned long long)right_timeline.matched_by_activity_id,
        (unsigned long long)right_timeline.matched_by_process_thread,
        (unsigned long long)right_timeline.incomplete,
        (unsigned long long)right_timeline.unmatched,
        (unsigned long long)right_timeline.missing_call_trace_ids,
        (unsigned long long)right_timeline.missing_activity_ids,
        (unsigned long long)right_timeline.failed
    );

    wprintf(
        L"OLE Timeline: left matched=%llu by_activity_id=%llu by_process_thread_operation=%llu incomplete=%llu unmatched=%llu activity_sequences=%llu missing_activity_id=%llu; right matched=%llu by_activity_id=%llu by_process_thread_operation=%llu incomplete=%llu unmatched=%llu activity_sequences=%llu missing_activity_id=%llu\n",
        (unsigned long long)left_ole_timeline.matched,
        (unsigned long long)left_ole_timeline.matched_by_activity_id,
        (unsigned long long)left_ole_timeline.matched_by_process_thread,
        (unsigned long long)left_ole_timeline.incomplete,
        (unsigned long long)left_ole_timeline.unmatched,
        (unsigned long long)left_ole_timeline.activity_sequences,
        (unsigned long long)left_ole_timeline.missing_activity_ids,
        (unsigned long long)right_ole_timeline.matched,
        (unsigned long long)right_ole_timeline.matched_by_activity_id,
        (unsigned long long)right_ole_timeline.matched_by_process_thread,
        (unsigned long long)right_ole_timeline.incomplete,
        (unsigned long long)right_ole_timeline.unmatched,
        (unsigned long long)right_ole_timeline.activity_sequences,
        (unsigned long long)right_ole_timeline.missing_activity_ids
    );

    wprintf(L"Operation timings\n");
    for (index = 0U; index < comparison->left.operation_timing_count; ++index) {
        const COMTRACE_OPERATION_TIMING *left =
            &comparison->left.operation_timings[index];
        const COMTRACE_OPERATION_TIMING *right =
            find_operation_timing(&comparison->right, left->operation);

        wprintf(
            L"  operation=%ls left_calls=%llu left_avg_ms=%.3f right_calls=%llu right_avg_ms=%.3f\n",
            left->operation,
            (unsigned long long)left->call_count,
            left->average_duration_ms,
            (unsigned long long)(right == NULL ? 0ULL : right->call_count),
            right == NULL ? 0.0 : right->average_duration_ms
        );
    }

    for (index = 0U; index < comparison->right.operation_timing_count; ++index) {
        const COMTRACE_OPERATION_TIMING *right =
            &comparison->right.operation_timings[index];

        if (find_operation_timing(&comparison->left, right->operation) == NULL) {
            wprintf(
                L"  operation=%ls left_calls=0 left_avg_ms=0.000 right_calls=%llu right_avg_ms=%.3f\n",
                right->operation,
                (unsigned long long)right->call_count,
                right->average_duration_ms
            );
        }
    }
}

static void write_json_string(const wchar_t *value)
{
    const wchar_t *current;

    putwchar(L'"');
    if (value != NULL) {
        for (current = value; *current != L'\0'; ++current) {
            switch (*current) {
            case L'"':
                wprintf(L"\\\"");
                break;
            case L'\\':
                wprintf(L"\\\\");
                break;
            case L'\b':
                wprintf(L"\\b");
                break;
            case L'\f':
                wprintf(L"\\f");
                break;
            case L'\n':
                wprintf(L"\\n");
                break;
            case L'\r':
                wprintf(L"\\r");
                break;
            case L'\t':
                wprintf(L"\\t");
                break;
            default:
                if (*current < 0x20) {
                    wprintf(L"\\u%04x", (unsigned int)*current);
                } else {
                    putwchar(*current);
                }
                break;
            }
        }
    }
    putwchar(L'"');
}

static void write_json_object_observations(
    const COMTRACE_RELATIONSHIP_GRAPH *graph
)
{
    size_t index;
    size_t written = 0U;

    wprintf(L"[");
    if (graph == NULL) {
        wprintf(L"]");
        return;
    }

    for (index = 0U; index < graph->node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node = &graph->nodes[index];

        if (node->type != COMTRACE_GRAPH_NODE_COM_OBJECT) {
            continue;
        }

        wprintf(
            L"%ls      {\"process_id\": %lu, \"oid\": \"0x%llx\", \"observations\": %llu",
            written == 0U ? L"\n" : L",\n",
            (unsigned long)node->process_id,
            (unsigned long long)node->value,
            (unsigned long long)node->observation_count
        );

        if (node->has_observed_timestamp) {
            wprintf(
                L", \"first_seen_ticks\": \"%lld\", \"last_seen_ticks\": \"%lld\"",
                (long long)node->first_seen_timestamp,
                (long long)node->last_seen_timestamp
            );
        } else {
            wprintf(L", \"first_seen_ticks\": null, \"last_seen_ticks\": null");
        }

        wprintf(L"}");
        ++written;
    }

    if (written != 0U) {
        wprintf(L"\n    ");
    }
    wprintf(L"]");
}

void comtrace_trace_comparison_write_json(
    const COMTRACE_TRACE_COMPARISON *comparison,
    const wchar_t *left_path,
    const wchar_t *right_path
)
{
    size_t index;
    COMTRACE_TIMELINE_COUNTS left_com;
    COMTRACE_TIMELINE_COUNTS right_com;
    COMTRACE_OLE_TIMELINE_COUNTS left_ole;
    COMTRACE_OLE_TIMELINE_COUNTS right_ole;

    if (comparison == NULL) {
        return;
    }

    left_com = get_timeline_counts(&comparison->left);
    right_com = get_timeline_counts(&comparison->right);
    left_ole = get_ole_timeline_counts(&comparison->left);
    right_ole = get_ole_timeline_counts(&comparison->right);

    wprintf(L"{\n  \"schema_version\": 1,\n  \"left\": {\"path\": ");
    write_json_string(left_path);
    wprintf(
        L", \"events\": %llu, \"event_types\": %llu},\n  \"right\": {\"path\": ",
        (unsigned long long)comparison->left.total_events,
        (unsigned long long)comparison->left.event_catalog.count
    );
    write_json_string(right_path);
    wprintf(
        L", \"events\": %llu, \"event_types\": %llu},\n",
        (unsigned long long)comparison->right.total_events,
        (unsigned long long)comparison->right.event_catalog.count
    );
    wprintf(
        L"  \"summary\": {\"event_types_added\": %llu, \"event_types_removed\": %llu, \"event_types_count_changed\": %llu, \"objects_added\": %llu, \"objects_removed\": %llu, \"objects_changed\": %llu},\n",
        (unsigned long long)comparison->added_event_types,
        (unsigned long long)comparison->removed_event_types,
        (unsigned long long)comparison->changed_event_types,
        (unsigned long long)comparison->added_objects,
        (unsigned long long)comparison->removed_objects,
        (unsigned long long)comparison->changed_objects
    );

    wprintf(
        L"  \"timeline_summary\": {"
        L"\"left_com\": {\"matched\": %llu, \"matched_by_call_trace_id\": %llu, \"matched_by_activity_id\": %llu, \"matched_by_process_thread\": %llu, \"incomplete\": %llu, \"unmatched\": %llu, \"missing_call_trace_ids\": %llu, \"missing_activity_ids\": %llu, \"failed\": %llu}, "
        L"\"right_com\": {\"matched\": %llu, \"matched_by_call_trace_id\": %llu, \"matched_by_activity_id\": %llu, \"matched_by_process_thread\": %llu, \"incomplete\": %llu, \"unmatched\": %llu, \"missing_call_trace_ids\": %llu, \"missing_activity_ids\": %llu, \"failed\": %llu}, "
        L"\"left_ole\": {\"matched\": %llu, \"matched_by_activity_id\": %llu, \"matched_by_process_thread\": %llu, \"incomplete\": %llu, \"unmatched\": %llu, \"activity_sequences\": %llu, \"missing_activity_ids\": %llu}, "
        L"\"right_ole\": {\"matched\": %llu, \"matched_by_activity_id\": %llu, \"matched_by_process_thread\": %llu, \"incomplete\": %llu, \"unmatched\": %llu, \"activity_sequences\": %llu, \"missing_activity_ids\": %llu}},\n",
        (unsigned long long)left_com.matched,
        (unsigned long long)left_com.matched_by_call_trace_id,
        (unsigned long long)left_com.matched_by_activity_id,
        (unsigned long long)left_com.matched_by_process_thread,
        (unsigned long long)left_com.incomplete,
        (unsigned long long)left_com.unmatched,
        (unsigned long long)left_com.missing_call_trace_ids,
        (unsigned long long)left_com.missing_activity_ids,
        (unsigned long long)left_com.failed,
        (unsigned long long)right_com.matched,
        (unsigned long long)right_com.matched_by_call_trace_id,
        (unsigned long long)right_com.matched_by_activity_id,
        (unsigned long long)right_com.matched_by_process_thread,
        (unsigned long long)right_com.incomplete,
        (unsigned long long)right_com.unmatched,
        (unsigned long long)right_com.missing_call_trace_ids,
        (unsigned long long)right_com.missing_activity_ids,
        (unsigned long long)right_com.failed,
        (unsigned long long)left_ole.matched,
        (unsigned long long)left_ole.matched_by_activity_id,
        (unsigned long long)left_ole.matched_by_process_thread,
        (unsigned long long)left_ole.incomplete,
        (unsigned long long)left_ole.unmatched,
        (unsigned long long)left_ole.activity_sequences,
        (unsigned long long)left_ole.missing_activity_ids,
        (unsigned long long)right_ole.matched,
        (unsigned long long)right_ole.matched_by_activity_id,
        (unsigned long long)right_ole.matched_by_process_thread,
        (unsigned long long)right_ole.incomplete,
        (unsigned long long)right_ole.unmatched,
        (unsigned long long)right_ole.activity_sequences,
        (unsigned long long)right_ole.missing_activity_ids
    );

    wprintf(L"  \"event_type_changes\": [");
    for (index = 0U; index < comparison->event_type_diff_count; ++index) {
        const COMTRACE_EVENT_TYPE_DIFF *diff =
            &comparison->event_type_diffs[index];

        wprintf(
            L"%ls    {\"provider_id\": \"",
            index == 0U ? L"\n" : L",\n"
        );
        print_guid_text(&diff->provider_id);
        putwchar(L'"');
        wprintf(L", ");
        write_json_string(L"event_id");
        wprintf(L": %u, ", (unsigned int)diff->descriptor.Id);
        write_json_string(L"version");
        wprintf(L": %u, ", (unsigned int)diff->descriptor.Version);
        write_json_string(L"channel");
        wprintf(L": %u, ", (unsigned int)diff->descriptor.Channel);
        write_json_string(L"level");
        wprintf(L": %u, ", (unsigned int)diff->descriptor.Level);
        write_json_string(L"opcode");
        wprintf(L": %u, ", (unsigned int)diff->descriptor.Opcode);
        write_json_string(L"task");
        wprintf(L": %u, ", (unsigned int)diff->descriptor.Task);
        write_json_string(L"keyword");
        wprintf(L": ");
        {
            wchar_t keyword_text[19];

            swprintf_s(
                keyword_text,
                sizeof(keyword_text) / sizeof(keyword_text[0]),
                L"0x%llx",
                (unsigned long long)diff->descriptor.Keyword
            );
            write_json_string(keyword_text);
        }
        wprintf(L", ");
        write_json_string(L"left_count");
        wprintf(L": %llu, ", (unsigned long long)diff->left_count);
        write_json_string(L"right_count");
        wprintf(L": %llu}", (unsigned long long)diff->right_count);
    }
    if (comparison->event_type_diff_count != 0U) {
        wprintf(L"\n  ");
    }
    wprintf(L"],\n");

    wprintf(L"  \"object_changes\": [");
    for (index = 0U; index < comparison->object_diff_count; ++index) {
        const COMTRACE_OBJECT_DIFF *diff = &comparison->object_diffs[index];

        wprintf(
            L"%ls    {\"process_id\": %lu, \"oid\": \"0x%llx\", \"left_present\": %ls, \"right_present\": %ls, \"changed_fields\": \"0x%lx\"}",
            index == 0U ? L"\n" : L",\n",
            (unsigned long)diff->process_id,
            (unsigned long long)diff->object_id,
            diff->present_in_left ? L"true" : L"false",
            diff->present_in_right ? L"true" : L"false",
            (unsigned long)diff->changed_fields
        );
    }
    if (comparison->object_diff_count != 0U) {
        wprintf(L"\n  ");
    }
    wprintf(L"],\n");

    wprintf(L"  \"object_observations\": {\n    \"left\": ");
    write_json_object_observations(&comparison->left.relationship_graph);
    wprintf(L",\n    \"right\": ");
    write_json_object_observations(&comparison->right.relationship_graph);
    wprintf(L"\n  },\n");

    wprintf(L"  \"timeline\": {\n    \"left\": [");
    for (index = 0U; index < comparison->left.operation_timing_count; ++index) {
        const COMTRACE_OPERATION_TIMING *timing =
            &comparison->left.operation_timings[index];

        wprintf(
            L"%ls      {\"operation\": ",
            index == 0U ? L"\n" : L",\n"
        );
        write_json_string(timing->operation);
        wprintf(
            L", \"calls\": %llu, \"failed\": %llu, \"average_duration_ms\": %.3f, \"maximum_duration_ms\": %.3f}",
            (unsigned long long)timing->call_count,
            (unsigned long long)timing->failed_count,
            timing->average_duration_ms,
            timing->maximum_duration_ms
        );
    }
    if (comparison->left.operation_timing_count != 0U) {
        wprintf(L"\n    ");
    }
    wprintf(L"],\n    \"right\": [");
    for (index = 0U; index < comparison->right.operation_timing_count; ++index) {
        const COMTRACE_OPERATION_TIMING *timing =
            &comparison->right.operation_timings[index];

        wprintf(
            L"%ls      {\"operation\": ",
            index == 0U ? L"\n" : L",\n"
        );
        write_json_string(timing->operation);
        wprintf(
            L", \"calls\": %llu, \"failed\": %llu, \"average_duration_ms\": %.3f, \"maximum_duration_ms\": %.3f}",
            (unsigned long long)timing->call_count,
            (unsigned long long)timing->failed_count,
            timing->average_duration_ms,
            timing->maximum_duration_ms
        );
    }
    if (comparison->right.operation_timing_count != 0U) {
        wprintf(L"\n    ");
    }
    wprintf(L"]\n  }\n}\n");
}
