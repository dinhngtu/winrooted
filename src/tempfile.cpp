// Copyright 2010 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\tempfile.go

#include "stdafx.h"

#include "path_windows_lite.hpp"

namespace winrooted::detail {

std::wstring joinPath(std::wstring_view dir, std::wstring_view name) {
    std::wstring result;
    if (!dir.empty() && filepathlite::IsPathSeparator(dir.back())) {
        result = std::wstring(dir);
        result += name;
        return result;
    }
    result = std::wstring(dir);
    result += L'\\';
    result += name;
    return result;
}

} // namespace winrooted::detail
