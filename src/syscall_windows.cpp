// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\syscall\syscall_windows.go

#include "stdafx.h"

#include "golang_compat.hpp"
#include "syscall_windows.hpp"

#include <wil/result_macros.h>
#include <wil/resource.h>

namespace winrooted::detail {

HRESULT Open(PCWSTR name, ULONGLONG flag, ULONG perm, wil::unique_hfile &result) noexcept {
    if (!name || *name == 0) {
        RETURN_WIN32(ERROR_PATH_NOT_FOUND);
    }
    auto accessFlags = flag & (O_RDONLY | O_WRONLY | O_RDWR);
    ULONG access;
    switch (accessFlags) {
    case O_RDONLY:
        access = GENERIC_READ;
        break;
    case O_WRONLY:
        access = GENERIC_WRITE;
        break;
    case O_RDWR:
        access = GENERIC_READ | GENERIC_WRITE;
        break;
    }
    if ((flag & O_CREAT) != 0) {
        access |= GENERIC_WRITE;
    }
    if ((flag & O_APPEND) != 0) {
        // Remove GENERIC_WRITE unless O_TRUNC is set, in which case we need it to truncate the file.
        // We can't just remove FILE_WRITE_DATA because GENERIC_WRITE without FILE_WRITE_DATA
        // starts appending at the beginning of the file rather than at the end.
        if ((flag & O_TRUNC) == 0) {
            access &= ~(ULONG)GENERIC_WRITE;
        }
        // Set all access rights granted by GENERIC_WRITE except for FILE_WRITE_DATA.
        access |= FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | STANDARD_RIGHTS_WRITE | SYNCHRONIZE;
    }
    ULONG sharemode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    SECURITY_ATTRIBUTES sa{sizeof(sa)};
    if ((flag & O_CLOEXEC) == 0) {
        sa.bInheritHandle = TRUE;
    }
    ULONG attrs = FILE_ATTRIBUTE_NORMAL;
    if ((perm & S_IWRITE) == 0) {
        attrs = FILE_ATTRIBUTE_READONLY;
    }
    // fileFlags contains the high 12 bits of flag.
    // This bit range can be used by the caller to specify the file flags
    // passed to CreateFile. It is an error to use if the bits can't be
    // mapped to the supported FILE_FLAG_* constants.
    ULONG fileFlags = flag & FileFlagsMask;
    if ((fileFlags & ~ValidFileFlagsMask) == 0) {
        attrs |= fileFlags;
    } else {
        RETURN_WIN32(ERROR_INVALID_HANDLE);
    }

    switch (accessFlags) {
    case O_WRONLY:
    case O_RDWR:
        // Unix doesn't allow opening a directory with O_WRONLY
        // or O_RDWR, so we don't set the flag in that case,
        // which will make CreateFile fail with ERROR_ACCESS_DENIED.
        // We will map that to EISDIR if the file is a directory.
        break;
    default:
        // We might be opening a directory for reading,
        // and CreateFile requires FILE_FLAG_BACKUP_SEMANTICS
        // to work with directories.
        attrs |= FILE_FLAG_BACKUP_SEMANTICS;
        break;
    }
    if ((flag & O_SYNC) != 0) {
        attrs |= FILE_FLAG_WRITE_THROUGH;
    }
    // We don't use CREATE_ALWAYS, because when opening a file with
    // FILE_ATTRIBUTE_READONLY these will replace an existing file
    // with a new, read-only one. See https://go.dev/issue/38225.
    //
    // Instead, we ftruncate the file after opening when O_TRUNC is set.
    ULONG createmode;
    if ((flag & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
        createmode = CREATE_NEW;
        attrs |= FILE_FLAG_OPEN_REPARSE_POINT; // don't follow symlinks
    } else if ((flag & O_CREAT) == O_CREAT) {
        createmode = OPEN_ALWAYS;
    } else {
        createmode = OPEN_EXISTING;
    }
    wil::unique_hfile h(CreateFile(name, access, sharemode, &sa, createmode, attrs, 0));
    auto err = GetLastError();
    if (!h.is_valid()) {
        if (err == ERROR_ACCESS_DENIED && (attrs & FILE_FLAG_BACKUP_SEMANTICS) == 0) {
            // We should return EISDIR when we are trying to open a directory with write access.
            auto fa = GetFileAttributes(name);
            if (fa != INVALID_FILE_ATTRIBUTES && (fa & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                err = ERROR_DIRECTORY_NOT_SUPPORTED;
            }
        }
        RETURN_WIN32(err);
    }
    if ((flag & O_DIRECTORY) != 0) {
        // Check if the file is a directory, else return ENOTDIR.
        BY_HANDLE_FILE_INFORMATION fi;
        RETURN_IF_WIN32_BOOL_FALSE(GetFileInformationByHandle(h.get(), &fi));
        if ((fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            RETURN_WIN32(ERROR_DIRECTORY);
        }
    }
    // Ignore O_TRUNC if the file has just been created.
    if ((flag & O_TRUNC) == O_TRUNC &&
        (createmode == OPEN_EXISTING || (createmode == OPEN_ALWAYS && err == ERROR_ALREADY_EXISTS))) {
        auto hr = Ftruncate(h.get(), 0);
        if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)) {
            // ERROR_INVALID_PARAMETER means truncation is not supported on this file handle.
            // Unix's O_TRUNC specification says to ignore O_TRUNC on named pipes and terminal devices.
            // We do the same here.
            auto t = GetFileType(h.get());
            RETURN_HR_IF(hr, !(t == FILE_TYPE_PIPE || t == FILE_TYPE_CHAR));
        }
    }

    result = std::move(h);
    return S_OK;
}

HRESULT Ftruncate(HANDLE h, LONGLONG length) noexcept {
    FILE_END_OF_FILE_INFO info{};
    info.EndOfFile.QuadPart = length;

    RETURN_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(h, FileEndOfFileInfo, &info, sizeof(info)));
    return S_OK;
}

} // namespace winrooted::detail
