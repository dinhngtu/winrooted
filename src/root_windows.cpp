// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\root_windows.go

#include "stdafx.h"

#include "at_windows.hpp"
#include "file_posix.hpp"
#include "file_windows.hpp"
#include "golang_compat.hpp"
#include "path_windows.hpp"
#include "root_openat.hpp"
#include "root_windows.hpp"
#include "root.hpp"
#include "syscall_windows.hpp"
#include "tempfile.hpp"
#include "types_windows.hpp"

#include <wil/result_macros.h>
#include <wil/resource.h>

namespace winrooted::detail {

// openRootNolog is OpenRoot.
HRESULT Root::openRootNolog(std::wstring_view name, Root &result) noexcept try {
    if (name.empty()) {
        RETURN_WIN32(ERROR_PATH_NOT_FOUND);
    }
    std::wstring path;
    RETURN_IF_FAILED(fixLongPath(name, path));
    wil::unique_hfile fd;
    RETURN_IF_FAILED(winrooted::detail::Open(path.c_str(), O_RDONLY | O_CLOEXEC, 0, fd));
    result = Root(std::move(fd), name);
    return S_OK;
}
CATCH_RETURN();

// NOTICE: newRoot
// newRoot returns a new Root.
// If fd is not a directory, it closes it and returns an error.
Root::Root(wil::unique_hfile &&fd, std::wstring_view name) {
    // Check that this is a directory.
    //
    // If we get any errors here, ignore them; worst case we create a Root
    // which returns errors when you try to use it.
    BY_HANDLE_FILE_INFORMATION fi;
    THROW_IF_WIN32_BOOL_FALSE(GetFileInformationByHandle(fd.get(), &fi));
    if ((fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        THROW_WIN32(ERROR_DIRECTORY);
    }

    _fd = std::move(fd);
    _name = std::wstring(name);
}

// openRootInRoot is Root.OpenRoot.
HRESULT Root::openRootInRoot(std::wstring_view name, Root &result) const noexcept try {
    wil::unique_hfile fd;
    RETURN_IF_FAILED(doInRoot<wil::unique_hfile>(*this, name, nullptr, rootOpenDir, fd));
    result = Root(std::move(fd), joinPath(this->Name(), name));
    return S_OK;
}
CATCH_RETURN();

// rootOpenFileNolog is Root.OpenFile.
HRESULT Root::rootOpenFileNolog(std::wstring_view name, int flag, ULONG perm, wil::unique_hfile &file) const noexcept {
    wil::unique_hfile fd;
    RETURN_IF_FAILED(
        doInRoot<wil::unique_hfile>(
            *this,
            name,
            nullptr,
            [=](HANDLE parent, std::wstring_view name, wil::unique_hfile &result, std::wstring &link) {
                return openat(parent, name, flag, perm, result, link);
            },
            fd));
    // NOTICE: handle only
    // auto nonblocking = (flag & O_FILE_FLAG_OVERLAPPED) != 0;
    file = std::move(fd);
    return S_OK;
}

HRESULT openat(
    HANDLE dirfd,
    std::wstring_view name,
    ULONGLONG flag,
    ULONG perm,
    wil::unique_hfile &result,
    std::wstring &link) noexcept {
    HRESULT hr = Openat(dirfd, name, flag | O_CLOEXEC | O_NOFOLLOW_ANY, syscallMode(perm), result);
    if (hr == HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED) || hr == HRESULT_FROM_WIN32(ERROR_DIRECTORY)) {
        link.clear();
        if (SUCCEEDED(readReparseLinkAt(dirfd, name, link))) {
            RETURN_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED);
        }
    }
    return hr;
}

HRESULT readReparseLinkAt(HANDLE dirfd, std::wstring_view name, std::wstring &link) noexcept {
    ObjectAttributes objAttrs;
    RETURN_IF_FAILED(objAttrs.init(dirfd, name));
    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    RETURN_IF_NTSTATUS_FAILED(NtCreateFile(
        &h,
        FILE_GENERIC_READ,
        &objAttrs,
        &iosb,
        nullptr,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT,
        nullptr,
        0));
    return readReparseLinkHandle(h.get(), link);
}

HRESULT rootOpenDir(HANDLE parent, std::wstring_view name, wil::unique_hfile &result, std::wstring &link) noexcept {
    wil::unique_hfile h;
    HRESULT hr = openat(parent, name, O_RDONLY | O_CLOEXEC | O_DIRECTORY, 0, h, link);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        // Windows returns:
        //   - ERROR_PATH_NOT_FOUND if any path component before the leaf
        //     does not exist or is not a directory.
        //   - ERROR_FILE_NOT_FOUND if the leaf does not exist.
        //
        // This differs from Unix behavior, which is:
        //   - ENOENT if any path component does not exist, including the leaf.
        //   - ENOTDIR if any path component before the leaf is not a directory.
        //
        // We map syscall.ENOENT to ERROR_FILE_NOT_FOUND and syscall.ENOTDIR
        // to ERROR_PATH_NOT_FOUND, but the Windows errors don't quite match.
        //
        // For consistency with os.Open, convert ERROR_FILE_NOT_FOUND here into
        // ERROR_PATH_NOT_FOUND, since we're opening a non-leaf path component.
        RETURN_WIN32(ERROR_PATH_NOT_FOUND);
    }
    result = std::move(h);
    return S_OK;
}

} // namespace winrooted::detail
