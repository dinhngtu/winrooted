// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\os\file_windows.go

#include "stdafx.h"

#include <wil/result_macros.h>
#include <wil/resource.h>

namespace winrooted::detail {

HRESULT openSymlink(PCWSTR path, wil::unique_hfile &result) noexcept {
    ULONG attrs = FILE_FLAG_BACKUP_SEMANTICS;
    // Use FILE_FLAG_OPEN_REPARSE_POINT, otherwise CreateFile will follow symlink.
    // See
    // https://docs.microsoft.com/en-us/windows/desktop/FileIO/symbolic-link-effects-on-file-systems-functions#createfile-and-createfiletransacted
    attrs |= FILE_FLAG_OPEN_REPARSE_POINT;
    wil::unique_hfile h(CreateFileW(path, 0, 0, nullptr, OPEN_EXISTING, attrs, 0));
    RETURN_LAST_ERROR_IF(!h.is_valid());
    result = std::move(h);
    return S_OK;
}

int winreadlinkvolume = 0;
static _Interlocked_ LONG64 winreadlinkvolume_counter = 0;

HRESULT normaliseLinkPath(std::wstring_view path, std::wstring &result) noexcept {
    try {
        if (path.length() < 4 || path.substr(0, 4) != LR"(\??\)") {
            // unexpected path, return it as is
            result = path;
            return S_OK;
        }
        // we have path that start with \??\.
        auto s = path.substr(4);
        if (s.length() >= 2 && s[1] == ':') { // \??\C:\foo\bar
            result = s;
            return S_OK;
        } else if (s.length() >= 4 && s.substr(0, 4) == L"UNC\\") { // \??\UNC\foo\bar
            result = std::wstring(L"\\\\");
            result += s.substr(4);
            return S_OK;
        }

        // \??\Volume{abc}\.
        if (winreadlinkvolume != 0) {
            result = std::wstring(LR"(\\?\)");
            result += path.substr(4);
            return S_OK;
        }
        InterlockedIncrement64(&winreadlinkvolume_counter);

        wil::unique_hfile h;
        std::wstring pathStr(path);
        RETURN_IF_FAILED(openSymlink(pathStr.c_str(), h));

        std::wstring buf(100, L'\0');
        while (1) {
            auto n = GetFinalPathNameByHandleW(h.get(), buf.data(), static_cast<DWORD>(buf.size()), VOLUME_NAME_DOS);
            RETURN_LAST_ERROR_IF(!n);
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
                result = L"\\";
                result += s.substr(3);
                return S_OK;
            }
            result = s;
            return S_OK;
        }
        RETURN_HR_MSG(
            E_FAIL,
            "GetFinalPathNameByHandle returned unexpected path: %.*ws",
            static_cast<int>(s.length()),
            s.data());
    }
    CATCH_RETURN();
}

HRESULT readReparseLinkHandle(HANDLE h, std::wstring &link) noexcept {
    try {
        auto rdb = static_cast<PREPARSE_DATA_BUFFER>(calloc(1, MAXIMUM_REPARSE_DATA_BUFFER_SIZE));
        RETURN_IF_NULL_ALLOC(rdb);
        auto rdb_free = wil::scope_exit([&]() { free(rdb); });
        DWORD bytesReturned;
        RETURN_IF_WIN32_BOOL_FALSE(DeviceIoControl(
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
            auto substituteName = reinterpret_cast<PUCHAR>(rdb) + rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset;
            std::wstring s(
                reinterpret_cast<PWCHAR>(substituteName),
                rdb->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(wchar_t));
            if ((rdb->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE) != 0) {
                link = std::move(s);
                return S_OK;
            }
            RETURN_IF_FAILED(normaliseLinkPath(s, link));
            break;
        }
        case IO_REPARSE_TAG_MOUNT_POINT: {
            auto substituteName = reinterpret_cast<PUCHAR>(rdb) + rdb->MountPointReparseBuffer.SubstituteNameOffset;
            std::wstring s(
                reinterpret_cast<PWCHAR>(substituteName),
                rdb->MountPointReparseBuffer.SubstituteNameLength / sizeof(wchar_t));
            RETURN_IF_FAILED(normaliseLinkPath(s, link));
            break;
        }
        default:
            RETURN_WIN32(ERROR_PATH_NOT_FOUND);
        }
        return S_OK;
    }
    CATCH_RETURN();
}

} // namespace winrooted::detail
