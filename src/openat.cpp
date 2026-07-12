// Copyright 2009-2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "pch.h"

#include "objattrs.hpp"

#include <wil/result_macros.h>
#include <wil/resource.h>

namespace winrooted {

static const DWORD SupportedFileFlags = //
    FILE_FLAG_WRITE_THROUGH |           //
    FILE_FLAG_OVERLAPPED |              //
    FILE_FLAG_NO_BUFFERING |            //
    FILE_FLAG_RANDOM_ACCESS |           //
    FILE_FLAG_SEQUENTIAL_SCAN |         //
    FILE_FLAG_DELETE_ON_CLOSE |         //
    FILE_FLAG_BACKUP_SEMANTICS |        //
    FILE_FLAG_POSIX_SEMANTICS |         //
    FILE_FLAG_SESSION_AWARE |           //
    FILE_FLAG_OPEN_REPARSE_POINT |      //
    FILE_FLAG_OPEN_NO_RECALL;

static wil::unique_hfile OpenSymlink(PCWSTR path) {
    ULONG attrs = FILE_FLAG_BACKUP_SEMANTICS;
    // Use FILE_FLAG_OPEN_REPARSE_POINT, otherwise CreateFile will follow
    // symlink. See
    // https://docs.microsoft.com/en-us/windows/desktop/FileIO/symbolic-link-effects-on-file-systems-functions#createfile-and-createfiletransacted
    attrs |= FILE_FLAG_OPEN_REPARSE_POINT;
    wil::unique_hfile h(
        CreateFileW(path, 0, 0, nullptr, OPEN_EXISTING, attrs, 0));
    THROW_LAST_ERROR_IF(!h.is_valid());
    return h;
}

int winreadlinkvolume = 1;
static _Interlocked_ LONG64 winreadlinkvolume_counter = 0;

static std::wstring NormaliseLinkPath(std::wstring_view path) {
    if (path.length() < 4 || path.substr(0, 4) != LR"(\??\)") {
        // unexpected path, return it as is
        return std::wstring(path);
    }
    // we have path that start with \??\.
    auto s = path.substr(4);
    if (s.length() >= 2 && s[1] == ':') { // \??\C:\foo\bar
        return std::wstring(s);
    } else if (
        s.length() >= 4 && s.substr(0, 4) == L"UNC\\") { // \??\UNC\foo\bar
        std::wstring result(L"\\\\");
        result += s.substr(4);
        return result;
    }

    // \??\Volume{abc}\.
    if (winreadlinkvolume != 0) {
        std::wstring result(LR"(\\?\)");
        result += path.substr(4);
        return result;
    }
    InterlockedIncrement64(&winreadlinkvolume_counter);

    std::wstring pathStr(path);
    wil::unique_hfile h = OpenSymlink(pathStr.c_str());

    std::wstring buf(100, L'\0');
    while (1) {
        auto n = GetFinalPathNameByHandleW(
            h.get(),
            buf.data(),
            static_cast<DWORD>(buf.size()),
            VOLUME_NAME_DOS);
        THROW_LAST_ERROR_IF(!n);
        if (n < buf.size()) {
            break;
        }
        buf.resize(n);
    }
    s = buf;
    if (s.length() > 4 && s.substr(0, 4) == LR"(\\?\)") {
        s = s.substr(4);
        if (s.length() > 3 && s.substr(0, 3) == L"UNC") {
            // return path like \\server\share\...
            std::wstring result(L"\\");
            result += s.substr(3);
            return result;
        }
        return std::wstring(s);
    }
    THROW_HR_MSG(
        E_UNEXPECTED,
        "GetFinalPathNameByHandle returned unexpected path: %.*ws",
        static_cast<int>(s.length()),
        s.data());
}

static std::wstring ReadReparseLinkHandle(HANDLE h) {
    auto rdb = static_cast<PREPARSE_DATA_BUFFER>(
        calloc(1, MAXIMUM_REPARSE_DATA_BUFFER_SIZE));
    THROW_IF_NULL_ALLOC(rdb);
    auto rdb_free = wil::scope_exit([&]() { free(rdb); });
    DWORD bytesReturned;
    THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(
        h,
        FSCTL_GET_REPARSE_POINT,
        nullptr,
        0,
        rdb,
        MAXIMUM_REPARSE_DATA_BUFFER_SIZE,
        &bytesReturned,
        nullptr));

    switch (rdb->ReparseTag) {
    case IO_REPARSE_TAG_SYMLINK: {
        auto substituteName = reinterpret_cast<PUCHAR>(rdb) +
            rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset;
        std::wstring s(
            reinterpret_cast<PWCHAR>(substituteName),
            rdb->SymbolicLinkReparseBuffer.SubstituteNameLength /
                sizeof(wchar_t));
        if ((rdb->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE) !=
            0) {
            return s;
        }
        return NormaliseLinkPath(s);
    }
    case IO_REPARSE_TAG_MOUNT_POINT: {
        auto substituteName = reinterpret_cast<PUCHAR>(rdb) +
            rdb->MountPointReparseBuffer.SubstituteNameOffset;
        std::wstring s(
            reinterpret_cast<PWCHAR>(substituteName),
            rdb->MountPointReparseBuffer.SubstituteNameLength /
                sizeof(wchar_t));
        return NormaliseLinkPath(s);
    }
    default:
        THROW_WIN32(ERROR_PATH_NOT_FOUND);
    }
}

static std::wstring ReadReparseLinkAt(HANDLE dirfd, std::wstring_view name) {
    ObjectAttributes objAttrs(name, OBJ_DONT_REPARSE, dirfd);
    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    THROW_IF_NTSTATUS_FAILED(NtCreateFile(
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
    return ReadReparseLinkHandle(h.get());
}

// pair (file, information)
std::variant<wil::unique_hfile, std::wstring> OpenAtCore(
    HANDLE dirfd,
    std::wstring_view fileName,
    DWORD desiredAccess,
    DWORD shareMode,
    LPSECURITY_ATTRIBUTES securityAttributes,
    DWORD creationDisposition,
    DWORD flags,
    DWORD attributes,
    ULONG ntOptions) {
    ULONG objectAttributes = OBJ_DONT_REPARSE;
    ULONG ntDisposition;

    switch (creationDisposition) {
    case CREATE_NEW:
        ntDisposition = FILE_CREATE;
        break;
    case CREATE_ALWAYS:
        ntDisposition = FILE_OVERWRITE_IF;
        break;
    case OPEN_EXISTING:
        ntDisposition = FILE_OPEN;
        break;
    case OPEN_ALWAYS:
        ntDisposition = FILE_OPEN_IF;
        break;
    case TRUNCATE_EXISTING:
        ntDisposition = FILE_OPEN;
        break;
    default:
        THROW_WIN32(ERROR_INVALID_PARAMETER);
    }

    THROW_WIN32_IF(ERROR_INVALID_PARAMETER, (flags & ~SupportedFileFlags) != 0);
    if (flags & FILE_FLAG_WRITE_THROUGH) {
        ntOptions |= FILE_WRITE_THROUGH;
    }
    if (!(flags & FILE_FLAG_OVERLAPPED)) {
        ntOptions |= FILE_SYNCHRONOUS_IO_NONALERT;
    }
    if (flags & FILE_FLAG_NO_BUFFERING) {
        ntOptions |= FILE_NO_INTERMEDIATE_BUFFERING;
    }
    if (flags & FILE_FLAG_RANDOM_ACCESS) {
        ntOptions |= FILE_RANDOM_ACCESS;
    }
    if (flags & FILE_FLAG_SEQUENTIAL_SCAN) {
        ntOptions |= FILE_SEQUENTIAL_ONLY;
    }
    if (flags & FILE_FLAG_DELETE_ON_CLOSE) {
        desiredAccess |= DELETE;
        ntOptions |= FILE_DELETE_ON_CLOSE;
    }
    if (flags & FILE_FLAG_BACKUP_SEMANTICS) {
        ntOptions |= FILE_OPEN_FOR_BACKUP_INTENT;
    }
    if (!(flags & FILE_FLAG_POSIX_SEMANTICS)) {
        objectAttributes |= OBJ_CASE_INSENSITIVE;
    }
    if (flags & FILE_FLAG_SESSION_AWARE) {
        ntOptions |= FILE_SESSION_AWARE;
    }
    if (flags & FILE_FLAG_OPEN_REPARSE_POINT) {
        ntOptions |= FILE_OPEN_REPARSE_POINT;
    }
    if (flags & FILE_FLAG_OPEN_NO_RECALL) {
        ntOptions |= FILE_OPEN_NO_RECALL;
    }

    ObjectAttributes objAttrs(fileName, objectAttributes, dirfd);
    if (securityAttributes) {
        objAttrs.SecurityDescriptor = securityAttributes->lpSecurityDescriptor;
        if (securityAttributes->bInheritHandle) {
            objAttrs.Attributes |= OBJ_INHERIT;
        }
    }

    wil::unique_hfile h;
    IO_STATUS_BLOCK iosb;
    auto status = NtCreateFile(
        &h,
        desiredAccess,
        &objAttrs,
        &iosb,
        nullptr,
        attributes,
        shareMode,
        ntDisposition,
        ntOptions,
        nullptr,
        0);
    switch (status) {
    case STATUS_REPARSE_POINT_ENCOUNTERED:
    case STATUS_NOT_A_DIRECTORY:
        return ReadReparseLinkAt(dirfd, fileName);
    default:
        THROW_IF_NTSTATUS_FAILED(status);
        break;
    }

    if (creationDisposition == TRUNCATE_EXISTING) {
        FILE_END_OF_FILE_INFO info{};

        THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(
            h.get(),
            FileEndOfFileInfo,
            &info,
            sizeof(info)));
    }

    return std::move(h);
}

std::variant<wil::unique_hfile, std::wstring>
OpenDirAt(HANDLE parent, std::wstring_view name) {
    return OpenAtCore(
        parent,
        name,
        FILE_GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        FILE_ATTRIBUTE_NORMAL,
        FILE_DIRECTORY_FILE);
}

} // namespace winrooted
