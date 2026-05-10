#pragma once

#include <Windows.h>

namespace winrooted::detail {

// syscallMode returns the syscall-specific mode bits from Go's portable mode bits.
static constexpr ULONG syscallMode(ULONG i) {
    return i & 0777;
}

} // namespace winrooted::detail
