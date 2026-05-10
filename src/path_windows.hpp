#pragma once

#include <string>

#include <Windows.h>

namespace winrooted::detail {

HRESULT fixLongPath(std::wstring_view path, std::wstring &result) noexcept;
HRESULT addExtendedPrefix(std::wstring_view path, std::wstring &result) noexcept;

} // namespace winrooted::detail
