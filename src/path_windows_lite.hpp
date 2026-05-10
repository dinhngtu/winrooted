#pragma once

#include <string_view>
#include <string>

#include <Windows.h>

namespace winrooted::detail::filepathlite {

static inline bool IsPathSeparator(wchar_t c) noexcept {
    return c == L'\\' || c == L'/';
}

bool isLocal(std::wstring_view path) noexcept;
bool isReservedName(std::wstring_view name) noexcept;
bool isReservedBaseName(std::wstring_view name) noexcept;
bool IsAbs(std::wstring_view path) noexcept;
size_t volumeNameLen(std::wstring_view path) noexcept;
bool pathHasPrefixFold(std::wstring_view s, std::wstring_view prefix) noexcept;
size_t uncLen(std::wstring_view path, size_t prefixLen) noexcept;
bool cutPath(std::wstring_view path, std::wstring_view &first, std::wstring_view &next) noexcept;
HRESULT postClean(std::wstring &out, size_t volLen) noexcept;

} // namespace winrooted::detail::filepathlite
