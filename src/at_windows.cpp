// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\internal\syscall\windows\at_windows.go

#include "stdafx.h"

#include "at_windows.hpp"
#include "golang_compat.hpp"
#include "syscall_windows.hpp"
#include "types_windows.hpp"
#include "winrooted_internal.hpp"

namespace winrooted::detail {

HRESULT
Openat(HANDLE dirfd, std::wstring_view name, ULONGLONG flag, DWORD perm, wil::unique_hfile &result) noexcept {
    if (name.empty()) {
        RETURN_WIN32(ERROR_PATH_NOT_FOUND);
    }

    ACCESS_MASK access = 0;
    ULONG options = 0;
    // Map Win32 file flags to NT create options.
    ULONG fileFlags = static_cast<ULONG>(flag) & FileFlagsMask;
    if ((fileFlags & ~ValidFileFlagsMask) != 0) {
        RETURN_WIN32(ERROR_INVALID_HANDLE);
    }
    if ((fileFlags & O_FILE_FLAG_OVERLAPPED) == 0) {
        options |= FILE_SYNCHRONOUS_IO_NONALERT;
    }
    if ((fileFlags & O_FILE_FLAG_DELETE_ON_CLOSE) != 0) {
        access |= DELETE;
    }
    auto setOptionFlag = [&](ULONG ntFlag, ULONG win32Flag) {
        if ((fileFlags & win32Flag) != 0) {
            options |= ntFlag;
        }
    };
    setOptionFlag(FILE_NO_INTERMEDIATE_BUFFERING, O_FILE_FLAG_NO_BUFFERING);
    setOptionFlag(FILE_WRITE_THROUGH, O_FILE_FLAG_WRITE_THROUGH);
    setOptionFlag(FILE_SEQUENTIAL_ONLY, O_FILE_FLAG_SEQUENTIAL_SCAN);
    setOptionFlag(FILE_RANDOM_ACCESS, O_FILE_FLAG_RANDOM_ACCESS);
    setOptionFlag(FILE_OPEN_FOR_BACKUP_INTENT, O_FILE_FLAG_BACKUP_SEMANTICS);
    setOptionFlag(FILE_SESSION_AWARE, O_FILE_FLAG_SESSION_AWARE);
    setOptionFlag(FILE_DELETE_ON_CLOSE, O_FILE_FLAG_DELETE_ON_CLOSE);
    setOptionFlag(FILE_OPEN_NO_RECALL, O_FILE_FLAG_OPEN_NO_RECALL);
    setOptionFlag(FILE_OPEN_REPARSE_POINT, O_FILE_FLAG_OPEN_REPARSE_POINT);

    switch (flag & (O_RDONLY | O_WRONLY | O_RDWR)) {
    case O_RDONLY:
        // FILE_GENERIC_READ includes FILE_LIST_DIRECTORY.
        access |= FILE_GENERIC_READ;
        break;
    case O_WRONLY:
        access |= FILE_GENERIC_WRITE;
        options |= FILE_NON_DIRECTORY_FILE;
        break;
    case O_RDWR:
        access |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        options |= FILE_NON_DIRECTORY_FILE;
        break;
    default:
        // Stat opens files without requesting read or write permissions,
        // but we still need to request SYNCHRONIZE.
        access |= SYNCHRONIZE;
        break;
    }
    if ((flag & O_CREAT) != 0) {
        access |= FILE_GENERIC_WRITE;
    }
    if ((fileFlags & O_FILE_FLAG_NO_BUFFERING) != 0) {
        // Disable buffering implies no implicit append access.
        access &= ~(ACCESS_MASK)FILE_APPEND_DATA;
    }
    if ((flag & O_APPEND) != 0) {
        access |= FILE_APPEND_DATA;
        // Remove FILE_WRITE_DATA access unless O_TRUNC is set,
        // in which case we need it to truncate the file.
        if ((flag & O_TRUNC) == 0) {
            access &= ~FILE_WRITE_DATA;
        }
    }
    if ((flag & O_DIRECTORY) != 0) {
        options |= FILE_DIRECTORY_FILE;
        access |= FILE_LIST_DIRECTORY;
    }
    if ((flag & O_SYNC) != 0) {
        options |= FILE_WRITE_THROUGH;
    }
    if (flag & O_WRITE_ATTRS) {
        access |= FILE_WRITE_ATTRIBUTES;
    }
    // Allow File.Stat.
    access |= STANDARD_RIGHTS_READ | FILE_READ_ATTRIBUTES | FILE_READ_EA;

    ObjectAttributes objAttrs;
    if ((flag & O_NOFOLLOW_ANY) != 0) {
        objAttrs.Attributes |= OBJ_DONT_REPARSE;
    }
    if ((flag & O_CLOEXEC) == 0) {
        objAttrs.Attributes |= OBJ_INHERIT;
    }
    if ((fileFlags & O_FILE_FLAG_POSIX_SEMANTICS) == 0) {
        objAttrs.Attributes |= OBJ_CASE_INSENSITIVE;
    }
    RETURN_IF_FAILED(objAttrs.init(dirfd, name));

    // We don't use FILE_OVERWRITE/FILE_OVERWRITE_IF, because when opening
    // a file with FILE_ATTRIBUTE_READONLY these will replace an existing
    // file with a new, read-only one.
    //
    // Instead, we ftruncate the file after opening when O_TRUNC is set.
    ULONG disposition;
    if ((flag & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
        disposition = FILE_CREATE;
        options |= FILE_OPEN_REPARSE_POINT; // don't follow symlinks
    } else if ((flag & O_CREAT) == O_CREAT) {
        disposition = FILE_OPEN_IF;
    } else {
        disposition = FILE_OPEN;
    }

    ULONG fileAttrs = FILE_ATTRIBUTE_NORMAL;
    if ((perm & S_IWRITE) == 0) {
        fileAttrs = FILE_ATTRIBUTE_READONLY;
    }

    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    RETURN_IF_NTSTATUS_FAILED(NtCreateFile(
        &h,
        SYNCHRONIZE | access,
        &objAttrs,
        &iosb,
        nullptr,
        fileAttrs,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        disposition,
        FILE_OPEN_FOR_BACKUP_INTENT | options,
        nullptr,
        0));

    if ((flag & O_TRUNC) != 0) {
        HRESULT hr = Ftruncate(h.get(), 0);
        if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)) {
            // ERROR_INVALID_PARAMETER means truncation is not supported on this file handle.
            // Unix's O_TRUNC specification says to ignore O_TRUNC on named pipes and terminal devices.
            // We do the same here.
            auto t = GetFileType(h.get());
            RETURN_HR_IF(hr, !(t == FILE_TYPE_PIPE || t == FILE_TYPE_CHAR));
        } else {
            RETURN_IF_FAILED(hr);
        }
    }

    result = std::move(h);
    return S_OK;
}

HRESULT Mkdirat(HANDLE dirfd, std::wstring_view name, [[maybe_unused]] DWORD mode) noexcept {
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
        FILE_CREATE,
        FILE_DIRECTORY_FILE,
        nullptr,
        0));
    return S_OK;
}

// TestDeleteatFallback should only be used for testing purposes.
// When set, [Deleteat] uses the fallback path unconditionally.
bool TestDeleteatFallback = false;

static HRESULT deleteatFallback(wil::unique_hfile &h) noexcept;

HRESULT Deleteat(HANDLE dirfd, std::wstring_view name, DWORD options) noexcept {
    if (name == L".") {
        // NtOpenFile's documentation isn't explicit about what happens when deleting ".".
        // Make this an error consistent with that of POSIX.
        RETURN_WIN32(ERROR_INVALID_PARAMETER);
    }
    ObjectAttributes objAttrs;
    RETURN_IF_FAILED(objAttrs.init(dirfd, name));
    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status = NtOpenFile(
        &h,
        FILE_READ_ATTRIBUTES | DELETE,
        &objAttrs,
        &iosb,
        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_OPEN_REPARSE_POINT | FILE_OPEN_FOR_BACKUP_INTENT | options);
    if (!NT_SUCCESS(status)) {
        if (status != STATUS_ACCESS_DENIED) {
            RETURN_NTSTATUS(status);
        }

        // Access denied, try opening with DELETE only.
        // This may succeed if the file has restrictive permissions
        // but the caller has delete child permission on the parent directory.
        RETURN_IF_NTSTATUS_FAILED(NtOpenFile(
            &h,
            DELETE,
            &objAttrs,
            &iosb,
            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OPEN_REPARSE_POINT | FILE_OPEN_FOR_BACKUP_INTENT | options));
    }

    if (TestDeleteatFallback) {
        return deleteatFallback(h);
    }

    // First, attempt to delete the file using POSIX semantics
    // (which permit a file to be deleted while it is still open).
    // This matches the behavior of DeleteFileW.
    //
    // The following call uses features available on different Windows versions:
    // - FILE_DISPOSITION_INFORMATION_EX: Windows 10, version 1607 (aka RS1)
    // - FILE_DISPOSITION_POSIX_SEMANTICS: Windows 10, version 1607 (aka RS1)
    // - FILE_DISPOSITION_IGNORE_READONLY_ATTRIBUTE: Windows 10, version 1809 (aka RS5)
    //
    // Also, some file systems, like FAT32, don't support POSIX semantics.
    FILE_DISPOSITION_INFORMATION_EX infoEx = {
        .Flags = FILE_DISPOSITION_DELETE | FILE_DISPOSITION_POSIX_SEMANTICS |
            // This differs from DeleteFileW, but matches os.Remove's
            // behavior on Unix platforms of permitting deletion of
            // read-only files.
            FILE_DISPOSITION_IGNORE_READONLY_ATTRIBUTE,
    };
    status = NtSetInformationFile(h.get(), &iosb, &infoEx, sizeof(infoEx), FileDispositionInformationEx);
    RETURN_HR_IF_EXPECTED(S_OK, NT_SUCCESS(status));
    switch (status) {
    case STATUS_INVALID_INFO_CLASS: // the operating system doesn't support FileDispositionInformationEx
    case STATUS_INVALID_PARAMETER:  // the operating system doesn't support one of the flags
    case STATUS_NOT_SUPPORTED: // the file system doesn't support FILE_DISPOSITION_INFORMATION_EX or one of the flags
        return deleteatFallback(h);
    default:
        RETURN_NTSTATUS(status);
    }
}

// deleteatFallback is a deleteat implementation that strives
// for compatibility with older Windows versions and file systems
// over performance.
static HRESULT deleteatFallback(wil::unique_hfile &h) noexcept {
    BY_HANDLE_FILE_INFORMATION data;
    if (GetFileInformationByHandle(h.get(), &data) && (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0) {
        // Remove read-only attribute. Reopen the file, as it was previously open without FILE_WRITE_ATTRIBUTES access
        // in order to maximize compatibility in the happy path.
        wil::unique_hfile wh(ReOpenFile(
            h.get(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS));
        RETURN_LAST_ERROR_IF(!wh.is_valid());
        FILE_BASIC_INFO basic = {
            .FileAttributes = data.dwFileAttributes & ~(DWORD)FILE_ATTRIBUTE_READONLY,
        };
        IO_STATUS_BLOCK iosb;
        RETURN_IF_NTSTATUS_FAILED(NtSetInformationFile(wh.get(), &iosb, &basic, sizeof(basic), FileBasicInformation));
    }
    FILE_DISPOSITION_INFO disposition{.DeleteFile = TRUE};
    RETURN_IF_WIN32_BOOL_FALSE(
        SetFileInformationByHandle(h.get(), FileDispositionInfo, &disposition, sizeof(disposition)));
    return S_OK;
}

HRESULT Renameat(HANDLE olddirfd, std::wstring_view oldpath, HANDLE newdirfd, std::wstring_view newpath) noexcept {
    // Open source file
    ObjectAttributes objAttrs;
    RETURN_IF_FAILED(objAttrs.init(olddirfd, oldpath));
    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    RETURN_IF_NTSTATUS_FAILED(NtOpenFile(
        &h,
        SYNCHRONIZE | DELETE,
        &objAttrs,
        &iosb,
        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_OPEN_REPARSE_POINT | FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT));

    ULONG renameInfoExLen;
    auto renameInfoEx = make_file_rename_information_ex(newpath, &renameInfoExLen);
    RETURN_HR_IF(E_OUTOFMEMORY, !renameInfoEx);
    renameInfoEx->Flags = FILE_RENAME_REPLACE_IF_EXISTS | FILE_RENAME_POSIX_SEMANTICS;
    renameInfoEx->RootDirectory = newdirfd;

    NTSTATUS status =
        NtSetInformationFile(h.get(), &iosb, renameInfoEx.get(), renameInfoExLen, FileRenameInformationEx);
    RETURN_HR_IF_EXPECTED(S_OK, NT_SUCCESS(status));

    // If the prior rename failed, the filesystem might not support
    // POSIX semantics (for example, FAT), or might not have implemented
    // FILE_RENAME_INFORMATION_EX.
    //
    // Try again.
    ULONG renameInfoLen;
    auto renameInfo = make_file_rename_information(newpath, &renameInfoLen);
    RETURN_HR_IF(E_OUTOFMEMORY, !renameInfo);
    renameInfo->ReplaceIfExists = TRUE;
    renameInfo->RootDirectory = newdirfd;

    RETURN_IF_NTSTATUS_FAILED(
        NtSetInformationFile(h.get(), &iosb, renameInfo.get(), renameInfoLen, FileRenameInformation));
    return S_OK;
}

HRESULT Linkat(HANDLE olddirfd, std::wstring_view oldpath, HANDLE newdirfd, std::wstring_view newpath) noexcept {
    ObjectAttributes objAttrs;
    RETURN_IF_FAILED(objAttrs.init(olddirfd, oldpath));
    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    RETURN_IF_NTSTATUS_FAILED(NtOpenFile(
        &h,
        SYNCHRONIZE | FILE_WRITE_ATTRIBUTES,
        &objAttrs,
        &iosb,
        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_OPEN_REPARSE_POINT | FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT));

    ULONG linkInfoLen;
    auto linkInfo = make_file_link_information(newpath, &linkInfoLen);
    RETURN_HR_IF(E_OUTOFMEMORY, !linkInfo);
    linkInfo->RootDirectory = newdirfd;
    RETURN_IF_NTSTATUS_FAILED(NtSetInformationFile(h.get(), &iosb, linkInfo.get(), linkInfoLen, FileLinkInformation));
    return S_OK;
}

static HRESULT withPrivilege(PCWSTR privilege, std::function<HRESULT()> f) noexcept;
static HRESULT
symlinkat(std::wstring_view oldname, HANDLE newdirfd, std::wstring_view newname, SymlinkatFlags flags) noexcept;

HRESULT
Symlinkat(std::wstring_view oldname, HANDLE newdirfd, std::wstring_view newname, SymlinkatFlags flags) noexcept {
    // Temporarily acquire symlink-creating privileges if possible.
    // This is the behavior of CreateSymbolicLinkW.
    //
    // (When passed the SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE flag,
    // CreateSymbolicLinkW ignores errors in acquiring privileges, as we do here.)
    return withPrivilege(SE_CREATE_SYMBOLIC_LINK_NAME, [&]() { return symlinkat(oldname, newdirfd, newname, flags); });
}

static HRESULT
symlinkat(std::wstring_view oldname, HANDLE newdirfd, std::wstring_view newname, SymlinkatFlags flags) noexcept {
    ULONG options = 0;
    if ((flags & SYMLINKAT_DIRECTORY) != 0) {
        options |= FILE_DIRECTORY_FILE;
    } else {
        options |= FILE_NON_DIRECTORY_FILE;
    }

    ObjectAttributes objAttrs;
    RETURN_IF_FAILED(objAttrs.init(newdirfd, newname));
    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    RETURN_IF_NTSTATUS_FAILED(NtCreateFile(
        &h,
        SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | DELETE,
        &objAttrs,
        &iosb,
        nullptr,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_CREATE,
        FILE_OPEN_REPARSE_POINT | FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT | options,
        nullptr,
        0));

    ULONG rdbLen;
    auto rdb = make_reparse_data_buffer_symlink(oldname, {}, &rdbLen);
    RETURN_HR_IF(E_OUTOFMEMORY, !rdb);

    DWORD bytesReturned;
    BOOL success =
        DeviceIoControl(h.get(), FSCTL_SET_REPARSE_POINT, rdb.get(), rdbLen, nullptr, 0, &bytesReturned, nullptr);
    if (!success) {
        // Creating the symlink has failed, so try to remove the file.
        FILE_DISPOSITION_INFO disposition{.DeleteFile = TRUE};
        std::ignore =
            NtSetInformationFile(h.get(), &iosb, &disposition, sizeof(disposition), FileDispositionInformation);
        RETURN_LAST_ERROR();
    }

    return S_OK;
}

// withPrivilege temporariliy acquires the named privilege and runs f.
// If the privilege cannot be acquired it runs f anyway,
// which should fail with an appropriate error.
static HRESULT withPrivilege(PCWSTR privilege, std::function<HRESULT()> f) noexcept {
    if (!ImpersonateSelf(SecurityImpersonation)) {
        return f();
    }
    auto defer_revert_to_self = wil::scope_exit([] { FAIL_FAST_IF_WIN32_BOOL_FALSE(RevertToSelf()); });

    auto curThread = GetCurrentThread();
    wil::unique_handle token;
    if (!OpenThreadToken(curThread, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, FALSE, &token)) {
        return f();
    }

    TOKEN_PRIVILEGES tokenPriv;
    if (!LookupPrivilegeValueW(nullptr, privilege, &tokenPriv.Privileges[0].Luid)) {
        return f();
    }

    tokenPriv.PrivilegeCount = 1;
    tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(token.get(), FALSE, &tokenPriv, 0, nullptr, nullptr)) {
        return f();
    }

    return f();
}

} // namespace winrooted::detail
