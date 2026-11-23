# DDE Instrumentation

ComTrace provides an opt-in DDE instrumentation provider through the **ComTraceDdeInstrumentation** static library and **include/comtrace_dde.h**.

This is application instrumentation, not a system-wide DDEML hook. ComTrace does not automatically observe arbitrary calls to DdeConnect, DdeClientTransaction, or other DDEML functions. Applications that own the DDE client/server code call the instrumentation API at the corresponding lifecycle points.

## Capture

ComTrace.exe --capture enables the **ComTrace.DDE** provider. The provider is also listed by ComTrace.exe --providers when registered by an instrumented process.

Link the static library and include comtrace_dde.h. Call comtrace_dde_initialize before emitting events and pair each successful initialization with comtrace_dde_shutdown. Initialization is reference-counted and event writes are synchronized with shutdown.

## Identifiers

The library generates process-local identifiers for conversations and transaction instances:

- **ConversationId** identifies one local DDEML conversation.
- **TransactionInstanceId** identifies one local transaction start and completion pair.
- **DdeInstanceId** identifies the application's DDEML instance.
- **ConversationCorrelationId** is supplied by the application and connects client and server conversation records.
- **TransactionCorrelationId** is supplied by the application and connects a client transaction with the corresponding server callback.

The generated identifiers are not shared across processes. For cross-process correlation, the application must propagate the same nonzero correlation identifiers to both sides. ComTrace does not guess a relationship from matching item names or timestamps.

## Event lifecycle

The API records conversation open and close events, transaction start and completion events, and server callback events. It records transaction type, format, item name, result, success state, and data size. It does not record the DDE data payload.
Use `comtrace_dde_transaction_completed_ex` when the application can provide transaction type, format, and item at completion; the original `comtrace_dde_transaction_completed` remains available for callers that only have the original completion fields. Inspect compares metadata when both start and completion events provide it.

Conversation close events are paired within the same process using ConversationId. Transaction completion events are paired within the same process using ConversationId, role, and TransactionInstanceId.

Cross-process exchanges are correlated only when the client and server records have opposite roles and matching nonzero transaction correlation IDs. When supplied, conversation correlation IDs must also agree. Transaction type, format, and item name are checked when both sides provide them; item-name matching is case-insensitive. Missing optional metadata does not defeat an otherwise explicit correlation, but conflicting metadata does. Ambiguous cross-process exchange matches are reported and are not guessed. Ambiguous completion matches are counted separately from cross-process ambiguity so the later exchange-correlation pass cannot clear them. Reversed completion timestamps are retained and reported as invalid durations. Unmatched closes, completions, and metadata mismatches are counted separately.

## Example

The ComTraceDdeInstrumentationFixture target emits a representative client conversation, server conversation, client transaction, server callback, completion, and close sequence. Its ETL is inspected by the comtrace_dde_instrumentation integration test.

The fixture validates the provider and timeline pipeline, including a client transaction correlated with a server callback even when optional callback metadata is absent. The relationship graph represents callback observations as separate nodes and connects them to the originating transaction only through the explicit correlation result. It is not a substitute for instrumenting a real application's DDEML call sites.

## Limitations

- Applications must explicitly instrument their DDE client and server paths.
- Cross-process correlation requires the application to propagate correlation IDs.
- The provider records metadata and data size, not DDE payload contents.
- A trace without the custom provider enabled or without application instrumentation cannot provide a DDE timeline.
