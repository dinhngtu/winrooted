// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\internal\filepathlite\path.go

#include "stdafx.h"

#include "path_windows_lite.hpp"

#include <wil/result_macros.h>

namespace winrooted::detail::filepathlite {

static constexpr wchar_t Separator = L'\\';

// Clean is filepath.Clean.
HRESULT Clean(std::wstring_view path, std::wstring &result) noexcept {
    try {
        auto originalPath = path;
        auto volLen = volumeNameLen(path);
        path = path.substr(volLen);
        if (path.empty()) {
            if (volLen > 1 && IsPathSeparator(originalPath[0]) && IsPathSeparator(originalPath[1])) {
                // should be UNC
                result = std::wstring(originalPath);
                // NOTICE: FromSlash inlined
                std::replace(result.begin(), result.end(), L'/', Separator);
                return S_OK;
            }
            result = std::wstring(originalPath) + L".";
            return S_OK;
        }
        auto rooted = IsPathSeparator(path[0]);

        // NOTICE: lazybuf was not ported

        // Invariants:
        //	reading from path; r is index of next byte to process.
        //	writing to buf; w is index of next byte to write.
        //	dotdot is index in buf where .. must stop, either because
        //		it is the leading slash or it is a leading ../../.. prefix.
        auto n = path.length();
        std::wstring out;
        out.reserve(volLen + n + 2);
        size_t r = 0;
        size_t dotdot = 0;
        if (rooted) {
            out += Separator;
            r = dotdot = 1;
        }

        while (r < n) {
            if (IsPathSeparator(path[r])) {
                // empty path element
                r++;
            } else if (path[r] == L'.' && (r + 1 == n || IsPathSeparator(path[r + 1]))) {
                r++;
            } else if (path[r] == L'.' && path[r + 1] == L'.' && (r + 2 == n || IsPathSeparator(path[r + 2]))) {
                // .. element: remove to last separator
                r += 2;
                if (out.length() > dotdot) {
                    // can backtrack
                    out.pop_back();
                    while (out.length() > dotdot && !IsPathSeparator(out.back())) {
                        out.pop_back();
                    }
                } else if (!rooted) {
                    // cannot backtrack, but not rooted, so append .. element.
                    if (!out.empty()) {
                        out += Separator;
                    }
                    out += L"..";
                    dotdot = out.length();
                }
            } else {
                // real path element.
                // add slash if needed
                if ((rooted && out.length() != 1) || (!rooted && !out.empty())) {
                    out += Separator;
                }
                // copy element
                for (; r < n && !IsPathSeparator(path[r]); r++) {
                    out += path[r];
                }
            }
        }

        // Turn empty string into "."
        if (out.empty()) {
            out += L'.';
        }

        // NOTICE: check here instead of further in postClean
        if (out != path) {
            // NOTICE: postClean changed
            RETURN_IF_FAILED(postClean(out, volLen)); // avoid creating absolute paths on Windows
        }
        // NOTICE: FromSlash inlined
        std::replace(out.begin(), out.end(), L'/', Separator);
        result = out;
        return S_OK;
    }
    CATCH_RETURN();
}

} // namespace winrooted::detail::filepathlite
