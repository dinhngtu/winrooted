#pragma once

#include <string>

#include <Windows.h>

namespace winrooted::detail::filepathlite {

HRESULT Clean(std::wstring_view path, std::wstring &result) noexcept;

} // namespace winrooted::detail::filepathlite
