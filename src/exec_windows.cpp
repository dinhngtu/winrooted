// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\syscall\exec_windows.go

#include "stdafx.h"

#include "exec_windows.hpp"

#include <wil/result_macros.h>

namespace winrooted::detail {

HRESULT FullPath(PCWSTR name, std::wstring &result) noexcept {
    try {
        size_t n = 100;
        std::wstring buf;
        while (1) {
            buf = std::wstring(n, L'\0');
            n = GetFullPathNameW(name, static_cast<DWORD>(buf.length()), buf.data(), nullptr);
            RETURN_LAST_ERROR_IF(!n);
            if (n <= buf.size()) {
                buf.resize(n);
                result = buf;
                return S_OK;
            }
        }
    }
    CATCH_RETURN();
}

} // namespace winrooted::detail
