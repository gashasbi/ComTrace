#include "archive_checksum.h"

ULONG comtrace_archive_checksum_begin(void)
{
    return 0xFFFFFFFFUL;
}

ULONG comtrace_archive_checksum_update(
    ULONG state,
    const BYTE *data,
    size_t size
)
{
    size_t index;
    unsigned int bit;

    if (data == NULL && size != 0U) {
        return state;
    }

    for (index = 0U; index < size; ++index) {
        state ^= (ULONG)data[index];

        for (bit = 0U; bit < 8U; ++bit) {
            if ((state & 1U) != 0U) {
                state = (state >> 1U) ^ 0xEDB88320UL;
            } else {
                state >>= 1U;
            }
        }
    }

    return state;
}

ULONG comtrace_archive_checksum_finish(ULONG state)
{
    return state ^ 0xFFFFFFFFUL;
}

ULONG comtrace_archive_checksum(
    const BYTE *data,
    size_t size
)
{
    return comtrace_archive_checksum_finish(
        comtrace_archive_checksum_update(
            comtrace_archive_checksum_begin(),
            data,
            size
        )
    );
}
