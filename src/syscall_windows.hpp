#pragma once

#include <Windows.h>

#include <wil/resource.h>

namespace winrooted::detail {

HRESULT Open(PCWSTR name, ULONGLONG flag, ULONG perm, wil::unique_hfile &result) noexcept;
HRESULT Ftruncate(HANDLE h, LONGLONG length) noexcept;

} // namespace winrooted::detail
