// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\root_windows.go

#pragma once

#include "exec_windows.hpp"
#include "path_windows_lite.hpp"
#include "strings_lite.hpp"

#include <wil/result_macros.h>
#include <wil/resource.h>

namespace winrooted::detail {

static constexpr HRESULT errPathEscapes = HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);

// NOTICE: typing helper
template <typename It>
concept SplitPathIterator = std::input_iterator<It> && requires(std::wstring s, It it) { s += *it; };

// rootCleanPath uses GetFullPathName to perform lexical path cleaning.
//
// On Windows, file names are lexically cleaned at the start of a file operation.
// For example, on Windows the path `a\..\b` is exactly equivalent to `b` alone,
// even if `a` does not exist or is not a directory.
//
// We use the Windows API function GetFullPathName to perform this cleaning.
// We could do this ourselves, but there are a number of subtle behaviors here,
// and deferring to the OS maintains consistency.
// (For example, `a\.\` cleans to `a\`.)
//
// GetFullPathName operates on absolute paths, and our input path is relative.
// We make the path absolute by prepending a fixed prefix of \\?\?\.
//
// We want to detect paths which use .. components to escape the root.
// We do this by ensuring the cleaned path still begins with \\?\?\.
// We catch the corner case of a path which includes a ..\?\. component
// by rejecting any input paths which contain a ?, which is not a valid character
// in a Windows filename.
template <SplitPathIterator PrefixIt, SplitPathIterator SuffixIt>
HRESULT rootCleanPath(
    std::wstring_view s,
    PrefixIt prefixFirst,
    PrefixIt prefixLast,
    SuffixIt suffixFirst,
    SuffixIt suffixLast,
    std::wstring &result) noexcept try {
    // Reject paths which include a ? component (see above).
    if (stringslite::IndexByte(s, L'?') >= 0) {
        RETURN_WIN32(ERROR_INVALID_NAME);
    }

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

    std::wstring full;
    RETURN_IF_FAILED(FullPath(buf.c_str(), full));

    auto [cut, ok] = stringslite::CutPrefix(full, fixedPrefix);
    RETURN_HR_IF(errPathEscapes, !ok);
    auto trimmed = stringslite::TrimPrefix(cut, L"\\");
    if (trimmed.empty()) {
        trimmed = L".";
    }

    RETURN_HR_IF(errPathEscapes, !filepathlite::isLocal(trimmed));

    result = trimmed;
    return S_OK;
}
CATCH_RETURN();

HRESULT openat(
    HANDLE dirfd,
    std::wstring_view name,
    ULONGLONG flag,
    ULONG perm,
    wil::unique_hfile &result,
    std::wstring &link) noexcept;
HRESULT readReparseLinkAt(HANDLE dirfd, std::wstring_view name, std::wstring &link) noexcept;
HRESULT rootOpenDir(HANDLE parent, std::wstring_view name, wil::unique_hfile &result, std::wstring &link) noexcept;

} // namespace winrooted::detail
