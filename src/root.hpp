// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\root_openat.go
// From src\os\root_windows.go

#pragma once

#include <string>

#include <wil/resource.h>

namespace winrooted::detail {

// must match public def
// NOTICE: winrooted extension
// `WINROOTED_IN_ROOT_FUNC` must not throw a C++ exception. Doing so is fatal.
typedef HRESULT (*WINROOTED_IN_ROOT_FUNC)(
    // Handle of directory containing the last path element. Do not close this handle.
    _In_ HANDLE parent,
    // Name of the last path element.
    // Function may return `HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED)` to indicate that `name` is a symlink
    // which should be followed.
    _In_ PCWSTR name,
    // Context passed from `winrooted_Root_DoInRoot`.
    _Inout_opt_ PVOID context,
    // Path to symlink to be followed.
    // Must be valid iff function returns `HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED)`. Not doing so is fatal.
    // The link string will be freed with `free()`.
    _When_(return == __HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED), _Post_valid_ _At_(*link, _Post_notnull_))
        PWSTR *link);

class Root {
public:
    Root() {}

    // NOTICE: OpenRoot
    static HRESULT New(std::wstring_view name, Root &result) noexcept {
        return openRootNolog(name, result);
    }

    HANDLE Handle() const noexcept {
        return _fd.get();
    }

    const std::wstring &Name() const noexcept {
        return _name;
    }

    // NOTICE: winrooted extension
    HRESULT
    DoInRootAbi(std::wstring_view name, WINROOTED_IN_ROOT_FUNC func, PVOID context) const noexcept;

    HRESULT Open(std::wstring_view name, wil::unique_hfile &file) const noexcept;
    HRESULT Create(std::wstring_view name, wil::unique_hfile &file) const noexcept;
    HRESULT OpenFile(std::wstring_view name, int flag, ULONG perm, wil::unique_hfile &file) const noexcept;

private:
    static HRESULT openRootNolog(std::wstring_view name, Root &result) noexcept;
    HRESULT openRootInRoot(std::wstring_view name, Root &result) const noexcept;
    HRESULT rootOpenFileNolog(std::wstring_view name, int flag, ULONG perm, wil::unique_hfile &file) const noexcept;

    Root(wil::unique_hfile &&fd, std::wstring_view name);

    wil::unique_hfile _fd;
    std::wstring _name;
};

} // namespace winrooted::detail
