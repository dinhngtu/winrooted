#pragma once

#include <string_view>
#include <utility>

namespace winrooted::detail::stringslite {

bool HasPrefix(std::wstring_view s, std::wstring_view prefix) noexcept;
bool HasSuffix(std::wstring_view s, std::wstring_view suffix) noexcept;
long IndexByte(std::wstring_view s, wchar_t c) noexcept;
std::pair<std::wstring_view, bool> CutPrefix(std::wstring_view s, std::wstring_view prefix) noexcept;
std::pair<std::wstring_view, bool> CutSuffix(std::wstring_view s, std::wstring_view suffix) noexcept;
std::wstring_view TrimPrefix(std::wstring_view s, std::wstring_view prefix) noexcept;
std::wstring_view TrimSuffix(std::wstring_view s, std::wstring_view suffix) noexcept;

} // namespace winrooted::detail::stringslite
