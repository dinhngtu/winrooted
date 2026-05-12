// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\root_openat.go

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <Windows.h>

#include "path_windows_lite.hpp"
#include "root_windows.hpp"
#include "root.hpp"

#include <wil/result_macros.h>
#include <wil/resource.h>

namespace winrooted::detail {

static constexpr int rootMaxSymlinks = 8;

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
HRESULT splitPathInRoot(
    std::wstring_view s,
    PrefixIt prefixFirst,
    PrefixIt prefixLast,
    SuffixIt suffixFirst,
    SuffixIt suffixLast,
    std::vector<std::wstring> &parts,
    std::wstring &suffixSep) noexcept try {
    if (s.length() == 0) {
        RETURN_WIN32(ERROR_INVALID_NAME);
    }
    RETURN_HR_IF(errPathEscapes, filepathlite::IsPathSeparator(s[0]));

    std::wstring cleaned;
    // NOTICE: Windows
    if (true) {
        // Windows cleans paths before opening them.
        RETURN_IF_FAILED(rootCleanPath(s, prefixFirst, prefixLast, suffixFirst, suffixLast, cleaned));
        s = cleaned;
    }

    parts.clear();
    suffixSep.clear();
    size_t i = 0;
    size_t j = 1;
    while (1) {
        if (j < s.length() && !filepathlite::IsPathSeparator(s[j])) {
            // Keep looking for the end of this component.
            j++;
            continue;
        }
        parts.push_back(std::wstring(s.substr(i, j - i)));
        // Advance to the next component, or end of the path.
        auto partEnd = j;
        while (j < s.length() && filepathlite::IsPathSeparator(s[j])) {
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
    return S_OK;
}
CATCH_RETURN();

// NOTICE: typing helpers
// NOTICE: DoInRootFunc returns HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED) and populates `link` if `name`
// is a symlink.
template <typename Result>
using DoInRootFunc = std::function<HRESULT(HANDLE parent, std::wstring_view name, Result &result, std::wstring &link)>;
using OpenDirFunc = DoInRootFunc<wil::unique_hfile>;

// NOTICE: iterator helper
static constexpr auto nil = std::vector<std::wstring_view>::iterator{};

// doInRoot performs an operation on a path in a Root.
//
// It calls f with the FD or handle for the directory containing the last
// path element, and the name of the last path element.
//
// For example, given the path a/b/c it calls f with the FD for a/b and the name "c".
//
// If openDirFunc is non-nil, it is called to open intermediate path elements.
// For example, given the path a/b/c openDirFunc will be called to open a and a/b in turn.
//
// f or openDirFunc may return errSymlink to indicate that the path element is a symlink
// which should be followed. Note that this can result in f being called multiple times
// with different names. For example, give the path "link" which is a symlink to "target",
// f is called with the path "link", returns errSymlink("target"), and is called again with
// the path "target".
//
// If f or openDirFunc return a *PathError, doInRoot will set PathError.Path to the
// full path which caused the error.
template <typename Result>
HRESULT doInRoot(
    const Root &r,
    std::wstring_view name,
    OpenDirFunc &&openDirFunc,
    DoInRootFunc<Result> &&f,
    Result &result) noexcept try {
    std::vector<std::wstring> parts;
    std::wstring suffixSep;
    RETURN_IF_FAILED(splitPathInRoot(name, nil, nil, nil, nil, parts, suffixSep));
    if (!openDirFunc) {
        openDirFunc = rootOpenDir;
    }

    auto rootfd = r.Handle();
    auto dirfd = rootfd;
    auto defer_dirfd = wil::scope_exit([&]() {
        if (dirfd != rootfd) {
            CloseHandle(dirfd);
        }
    });

    const int maxSteps = 255;
    const int maxRestarts = 8;
    size_t i = 0;
    int steps = 0, restarts = 0, symlinks = 0;

    while (true) {
        steps++;
        if (steps > maxSteps && restarts > maxRestarts) {
            RETURN_WIN32(ERROR_FILENAME_EXCED_RANGE);
        }

        const auto &comp = parts[i];

        if (comp == L"..") {
            // Resolve one or more parent ("..") path components.
            //
            // Rewrite the original path,
            // removing the elements eliminated by ".." components,
            // and start over from the beginning.
            restarts++;
            auto end = i + 1;
            while (end < parts.size() && parts[end] == L"..") {
                end++;
            }
            auto count = end - i;
            RETURN_HR_IF(errPathEscapes, count > i);
            parts.erase(parts.begin() + i - count, parts.begin() + end);
            if (parts.empty()) {
                parts.push_back(L".");
            }
            i = 0;
            if (dirfd != rootfd) {
                CloseHandle(dirfd);
            }
            dirfd = rootfd;
            continue;
        }

        HRESULT hr;
        std::wstring link;
        if (i == parts.size() - 1) {
            // This is the last path element.
            // Call f to decide what to do with it.
            // If f returns errSymlink, this element is a symlink
            // which should be followed.
            // suffixSep contains any trailing separator characters
            // which we rejoin to the final part at this time.
            std::wstring lastComp = parts[i] + suffixSep;
            hr = f(dirfd, std::wstring_view(lastComp), result, link);
            if (SUCCEEDED(hr)) {
                return hr;
            }
        } else {
            wil::unique_hfile h;
            hr = openDirFunc(dirfd, parts[i], h, link);
            if (SUCCEEDED(hr)) {
                if (dirfd != rootfd) {
                    CloseHandle(dirfd);
                }
                dirfd = h.release();
            }
        }

        if (SUCCEEDED(hr)) {
        } else if (hr == HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED)) {
            RETURN_HR_IF(hr, link.empty());
            symlinks++;
            RETURN_HR_IF(hr, symlinks > rootMaxSymlinks);
            std::vector<std::wstring> newparts;
            std::wstring newSuffixSep;
            RETURN_IF_FAILED(splitPathInRoot(
                link,
                parts.begin(),
                parts.begin() + i,
                parts.begin() + (i + 1),
                parts.end(),
                newparts,
                newSuffixSep));
            if (i == parts.size() - 1) {
                // suffixSep contains any trailing path separator characters
                // in the link target.
                // If we are replacing the remainder of the path, retain these.
                // If we're replacing some intermediate component of the path,
                // ignore them, since intermediate components must always be
                // directories.
                suffixSep = newSuffixSep;
            }
            if (newparts.size() < i || !std::equal(parts.begin(), parts.begin() + i, newparts.begin())) {
                // Some component in the path which we have already traversed
                // has changed. We need to restart parsing from the root.
                i = 0;
                if (dirfd != rootfd) {
                    CloseHandle(dirfd);
                }
                dirfd = rootfd;
            }
            parts = newparts;
            continue;
        } else {
            RETURN_HR(hr);
        }

        i++;
    }
}
CATCH_RETURN();

} // namespace winrooted::detail
