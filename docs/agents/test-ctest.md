<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## TEST (ctest)

The GoogleTest unit + bonding suite is gated on `ENABLE_UNITTESTS` (with
`ENABLE_CXX11`, ON by default); `ENABLE_TESTING` additionally builds the developer
test apps. Both are OFF by default. Run the full suite with:

```bash
cmake -B build -DENABLE_TESTING=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

On the CeraLive baseline derived from Haivision `1e4c908` plus
`SRTO_REORDERFREEZE`, the full gtest suite passes (1 disabled:
`CTimer.SleeptoAccuracy`). Note: `ENABLE_TESTING=ON` alone registers no ctest
tests — `ENABLE_UNITTESTS=ON` is what wires the gtest suite into ctest.

Socket tests keep one owner per handle. A `UniqueSocket` must not be bypassed by
raw-closing its handle, and an explicit owner close releases that handle while
still asserting the real `srt_close` result. `srt_close` preserves its existing
idempotent-close contract when a valid socket is retired by the garbage collector
between the public state check and internal close acquisition. For connected
pairs, register readiness before launching a fast peer, synchronize the worker,
and close every owner explicitly; finite transfers consume their known byte count
instead of using peer shutdown as an end marker. These lifecycle assertions are
required gates and must not be relaxed or retried away.
Connection-timeout tests enforce their upper timing bound against the async
connect-failure callback timestamp, not elapsed time after the waiting thread is
rescheduled, and also assert the epoll result, callback error, socket state, and
rejection reason.

