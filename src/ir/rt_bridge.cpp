#include "rt_bridge.hpp"

#include "rt_ref.h"

namespace basic {

namespace {
thread_local int g_error = -1;
}

void recordRuntimeError(int code) {
    if (g_error < 0) g_error = code;
}

int pendingRuntimeError() {
    return g_error;
}

int takeRuntimeError() {
    const int e = g_error;
    g_error = -1;
    return e;
}

}  // namespace basic

extern "C" void rt_error(int code) {
    basic::recordRuntimeError(code);
}
