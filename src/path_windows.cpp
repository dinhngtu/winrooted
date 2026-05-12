// Copyright 2011 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\path_windows.go

#include "stdafx.h"

#include "exec_windows.hpp"
#include "path_windows_lite.hpp"
#include "path_windows.hpp"

#include <wil/result_macros.h>

namespace winrooted::detail {

bool CanUseLongPaths = false;

// fixLongPath returns the extended-length (\\?\-prefixed) form of
// path when needed, in order to avoid the default 260 character file
// path limit imposed by Windows. If the path is short enough or already
// has the extended-length prefix, fixLongPath returns path unmodified.
// If the path is relative and joining it with the current working
// directory results in a path that is too long, fixLongPath returns
// the absolute path with the extended-length prefix.
//
// See https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file#maximum-path-length-limitation
HRESULT fixLongPath(std::wstring_view path, std::wstring &result) noexcept try {
    if (CanUseLongPaths) {
        result = std::wstring(path);
        return S_OK;
    }
    return addExtendedPrefix(path, result);
}
CATCH_RETURN();

// addExtendedPrefix adds the extended path prefix (\\?\) to path.
HRESULT addExtendedPrefix(std::wstring_view path, std::wstring &result) noexcept try {
    if (path.length() >= 4) {
        if (path.starts_with(LR"(\??\)")) {
            // Already extended with \??\.
            result = std::wstring(path);
            return S_OK;
        }
        if (filepathlite::IsPathSeparator(path[0]) && filepathlite::IsPathSeparator(path[1]) && path[2] == '?' &&
            filepathlite::IsPathSeparator(path[3])) {
            // Already extended with \\?\ or any combination of directory separators.
            result = std::wstring(path);
            return S_OK;
        }
    }

    // Do nothing (and don't allocate) if the path is "short".
    // Empirically (at least on the Windows Server 2013 builder),
    // the kernel is arbitrarily okay with < 248 bytes. That
    // matches what the docs above say:
    // "When using an API to create a directory, the specified
    // path cannot be so long that you cannot append an 8.3 file
    // name (that is, the directory name cannot exceed MAX_PATH
    // minus 12)." Since MAX_PATH is 260, 260 - 12 = 248.
    //
    // The MSDN docs appear to say that a normal path that is 248 bytes long
    // will work; empirically the path must be less then 248 bytes long.
    auto pathLength = path.length();

    if (pathLength < 248) {
        // Don't fix. (This is how Go 1.7 and earlier worked,
        // not automatically generating the \\?\ form)
        result = std::wstring(path);
        return S_OK;
    }

    bool isUNC = false;
    bool isDevice = false;
    if (path.length() >= 2 && filepathlite::IsPathSeparator(path[0]) && filepathlite::IsPathSeparator(path[1])) {
        if (path.length() >= 4 && path[2] == '.' && filepathlite::IsPathSeparator(path[3])) {
            // Starts with //./
            isDevice = true;
        } else {
            // Starts with //
            isUNC = true;
        }
    }

    std::wstring resolved(path);
    RETURN_IF_FAILED(FullPath(resolved.c_str(), resolved));

    if (isUNC) {
        // UNC path, prepend the \\?\UNC\ prefix.
        resolved = LR"(\\?\UNC\)" + resolved.substr(2);
    } else if (isDevice) {
        // Don't add the extended prefix to device paths, as it would
        // change its meaning.
    } else {
        resolved = LR"(\\?\)" + resolved;
    }

    result = resolved;
    return S_OK;
}
CATCH_RETURN();

} // namespace winrooted::detail
