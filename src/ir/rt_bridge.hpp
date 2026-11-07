#pragma once

// Bridge to the C reference runtime: rt_error() only records the error; the caller checks it after
// the call (no C++ exception travels through C code).

namespace basic {

// The pending runtime error, -1 if none; takeRuntimeError() also clears it.
int pendingRuntimeError();
int takeRuntimeError();
void recordRuntimeError(int code);

}  // namespace basic
