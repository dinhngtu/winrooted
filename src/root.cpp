// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\root.go

#include "stdafx.h"

#include "golang_compat.hpp"
#include "root_openat.hpp"
#include "root_windows.hpp"
#include "root.hpp"

#include <wil/result_macros.h>

namespace winrooted::detail {

HRESULT
Root::DoInRootAbi(std::wstring_view name, WINROOTED_IN_ROOT_FUNC func, PVOID context) const noexcept {
    return doInRoot<PVOID const>(
        *this,
        name,
        nullptr,
        [func](HANDLE parent, std::wstring_view newname, PVOID const &result, std::wstring &link) {
            try {
                std::wstring newnamestr(newname);
                PWSTR newlink = nullptr;
                HRESULT hr;

                try {
                    hr = func(parent, newnamestr.c_str(), result, &newlink);
                }
                CATCH_FAIL_FAST_MSG("WINROOTED_IN_ROOT_FUNC threw an unhandled exception");

                if (hr == HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED)) {
                    FAIL_FAST_IF_NULL_MSG(
                        newlink,
                        "WINROOTED_IN_ROOT_FUNC returned a null link with ERROR_REPARSE_POINT_ENCOUNTERED");
                    auto newlink_free = wil::scope_exit([=]() { free(newlink); });
                    link = std::wstring(newlink);
                } else {
                    link.clear();
                }

                RETURN_HR(hr);
            }
            CATCH_RETURN();
        },
        context);
}

// Open opens the named file in the root for reading.
// See [Open] for more details.
HRESULT Root::Open(std::wstring_view name, wil::unique_hfile &file) const noexcept {
    return this->OpenFile(name, O_RDONLY, 0, file);
}

// Create creates or truncates the named file in the root.
// See [Create] for more details.
HRESULT Root::Create(std::wstring_view name, wil::unique_hfile &file) const noexcept {
    return this->OpenFile(name, O_RDWR | O_CREATE | O_TRUNC, 0666, file);
}

// OpenFile opens the named file in the root.
// See [OpenFile] for more details.
//
// If perm contains bits other than the nine least-significant bits (0o777),
// OpenFile returns an error.
HRESULT Root::OpenFile(std::wstring_view name, int flag, ULONG perm, wil::unique_hfile &file) const noexcept {
    if ((perm & 0777) != perm) {
        RETURN_WIN32(ERROR_INVALID_PARAMETER);
    }
    // NOTICE: no logging
    // r.logOpen(name)
    wil::unique_hfile rf;
    RETURN_IF_FAILED(this->rootOpenFileNolog(name, flag, perm, rf));
    // NOTICE: handle only
    // rf.appendMode = flag&O_APPEND != 0
    file = std::move(rf);
    return S_OK;
}

} // namespace winrooted::detail
