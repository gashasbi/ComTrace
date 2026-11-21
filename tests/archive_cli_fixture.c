#include <windows.h>
#include <evntcons.h>
#include <wchar.h>
#include "archive_writer.h"

int wmain(int argc, wchar_t **argv)
{
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_SUMMARY summary;
    EVENT_RECORD event;
    COMTRACE_ARCHIVE_STATUS status;

    if ((argc != 2 && argc != 3) ||
        (argc == 3 && wcscmp(argv[2], L"alternate") != 0)) {
        return 1;
    }

    ZeroMemory(&writer, sizeof(writer));
    ZeroMemory(&summary, sizeof(summary));
    ZeroMemory(&event, sizeof(event));

    event.EventHeader.Size = 462U;
    event.EventHeader.ThreadId = 42U;
    event.EventHeader.ProcessId = 24U;
    event.EventHeader.TimeStamp.QuadPart = 123456789LL;
    event.EventHeader.EventDescriptor.Id = argc == 3 ? 92U : 91U;
    event.EventHeader.EventDescriptor.Version = 1U;
    event.EventHeader.EventDescriptor.Channel = 2U;
    event.EventHeader.EventDescriptor.Level = 4U;
    event.EventHeader.EventDescriptor.Opcode = 3U;
    event.EventHeader.EventDescriptor.Task = 5U;
    event.EventHeader.EventDescriptor.Keyword = 0x1122334455667788ULL;

    status = comtrace_archive_writer_open(&writer, argv[1]);
    if (status == COMTRACE_ARCHIVE_SUCCESS) {
        status = comtrace_archive_writer_write_event(&writer, &event);
    }

    summary.process_trace_status = ERROR_SUCCESS;
    summary.buffers_read = 1U;
    summary.buffers_written = 1U;
    summary.trace_clock_type = 1U;
    summary.pointer_size = 8U;

    if (status == COMTRACE_ARCHIVE_SUCCESS) {
        status = comtrace_archive_writer_finalize(&writer, &summary);
    }

    comtrace_archive_writer_destroy(&writer);
    return status == COMTRACE_ARCHIVE_SUCCESS ? 0 : 1;
}
