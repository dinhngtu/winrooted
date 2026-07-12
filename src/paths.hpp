// Copyright 2009-2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#include <string_view>
#include <string>
#include <utility>
#include <vector>

#include <Windows.h>

#include <wil/result_macros.h>

namespace winrooted {

static bool IsPathSeparator(wchar_t c) noexcept {
    return c == L'\\' || c == L'/';
}

static bool HasPrefix(std::wstring_view s, std::wstring_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

static std::pair<std::wstring_view, bool> CutPrefix(std::wstring_view s, std::wstring_view prefix) {
    if (!HasPrefix(s, prefix)) {
        return {s, false};
    }
    return {s.substr(prefix.size()), true};
}

static std::wstring_view TrimPrefix(std::wstring_view s, std::wstring_view prefix) {
    if (HasPrefix(s, prefix)) {
        return s.substr(prefix.size());
    }
    return s;
}

std::wstring FullPath(PCWSTR name);
bool IsLocal(std::wstring_view path);

// NOTICE: typing helper
template <typename It>
concept SplitPathIterator = std::input_iterator<It> && requires(std::wstring s, It it) { s += *it; };

template <SplitPathIterator PrefixIt, SplitPathIterator SuffixIt>
static std::wstring RootCleanPath(
    std::wstring_view s,
    PrefixIt prefixFirst,
    PrefixIt prefixLast,
    SuffixIt suffixFirst,
    SuffixIt suffixLast) {
    // Reject paths which include a ? component (see above).
    THROW_WIN32_IF(ERROR_INVALID_NAME, s.find(L'?') != std::string_view::npos);

    const auto fixedPrefix = LR"(\\?\?)";
    std::wstring buf = fixedPrefix;
    for (auto pit = prefixFirst; pit != prefixLast; pit++) {
        buf.push_back(L'\\');
        buf += *pit;
    }
    buf.push_back(L'\\');
    buf += s;
    for (auto sit = suffixFirst; sit != suffixLast; sit++) {
        buf.push_back(L'\\');
        buf += *sit;
    }

    auto full = FullPath(buf.c_str());

    auto [cut, ok] = CutPrefix(full, fixedPrefix);
    THROW_WIN32_IF(ERROR_ACCESS_DENIED, !ok);
    auto trimmed = TrimPrefix(cut, L"\\");
    if (trimmed.empty()) {
        trimmed = L".";
    }

    THROW_WIN32_IF(ERROR_ACCESS_DENIED, !IsLocal(trimmed));

    return std::wstring(trimmed);
}

// splitPathInRoot splits a path into components
// and joins it with the given prefix and suffix.
//
// The path is relative to a Root, and must not be
// absolute, volume-relative, or "".
//
// "." components are removed, except in the last component.
//
// Path separators following the last component are returned in suffixSep.
template <SplitPathIterator PrefixIt, SplitPathIterator SuffixIt>
static std::pair<std::vector<std::wstring>, std::wstring> SplitPathInRoot(
    std::wstring_view s,
    PrefixIt prefixFirst,
    PrefixIt prefixLast,
    SuffixIt suffixFirst,
    SuffixIt suffixLast) {
    THROW_WIN32_IF(ERROR_INVALID_NAME, s.empty());
    THROW_WIN32_IF(ERROR_ACCESS_DENIED, IsPathSeparator(s[0]));

    // Windows cleans paths before opening them.
    std::wstring cleaned = RootCleanPath(s, prefixFirst, prefixLast, suffixFirst, suffixLast);
    s = std::move(cleaned);

    std::vector<std::wstring> parts;
    std::wstring suffixSep;
    size_t i = 0;
    size_t j = 1;
    while (1) {
        if (j < s.length() && !IsPathSeparator(s[j])) {
            // Keep looking for the end of this component.
            j++;
            continue;
        }
        parts.push_back(std::wstring(s.substr(i, j - i)));
        // Advance to the next component, or end of the path.
        auto partEnd = j;
        while (j < s.length() && IsPathSeparator(s[j])) {
            j++;
        }
        if (j == s.length()) {
            // If this is the last path component,
            // preserve any trailing path separators.
            suffixSep = s.substr(partEnd);
            break;
        }
        if (parts.back() == L".") {
            // Remove "." components, except at the end.
            parts.pop_back();
        }
        i = j;
    }
    return std::make_pair(std::move(parts), std::move(suffixSep));
}

} // namespace winrooted
