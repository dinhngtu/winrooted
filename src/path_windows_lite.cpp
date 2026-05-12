// Copyright 2010 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\internal\filepathlite\path_windows.go

#include "stdafx.h"

#include "path_lite.hpp"
#include "path_windows_lite.hpp"
#include "strings_lite.hpp"

#include <wil/result_macros.h>

namespace winrooted::detail::filepathlite {

bool isLocal(std::wstring_view path) noexcept {
    if (path.empty()) {
        return false;
    }
    if (IsPathSeparator(path[0])) {
        return false;
    }
    if (stringslite::IndexByte(path, L':') >= 0) {
        // Colons are only valid when marking a drive letter ("C:foo").
        // Rejecting any path with a colon is conservative but safe.
        return false;
    }
    bool hasDots = false; // contains . or .. path elements
    for (auto p = path; !p.empty();) {
        std::wstring_view part;
        std::ignore = cutPath(p, part, p);
        if (part == L"." || part == L"..") {
            hasDots = true;
        }
        if (isReservedName(part)) {
            return false;
        }
    }
    if (hasDots) {
        std::wstring cleaned;
        FAIL_FAST_IF_FAILED(Clean(path, cleaned));
        path = cleaned;
    }
    if (path == L".." || stringslite::HasPrefix(path, L"..\\")) {
        return false;
    }
    return true;
}

// isReservedName reports if name is a Windows reserved device name.
// It does not detect names with an extension, which are also reserved on some Windows versions.
//
// For details, search for PRN in
// https://docs.microsoft.com/en-us/windows/desktop/fileio/naming-a-file.
bool isReservedName(std::wstring_view name) noexcept try {
    // Device names can have arbitrary trailing characters following a dot or colon.
    auto base = name;
    for (size_t i = 0; i < base.length(); i++) {
        switch (base[i]) {
        case L':':
        case L'.':
            base = base.substr(0, i);
            break;
        }
    }
    // Trailing spaces in the last path element are ignored.
    while (base.length() > 0 && base[base.length() - 1] == L' ') {
        base = base.substr(0, base.length() - 1);
    }
    if (!isReservedBaseName(base)) {
        return false;
    }
    if (base.length() == name.length()) {
        return true;
    }
    // The path element is a reserved name with an extension.
    // Since Windows 11, reserved names with extensions are no
    // longer reserved. For example, "CON.txt" is a valid file
    // name. Use RtlIsDosDeviceName_U to see if the name is reserved.
    std::wstring p(name);
    return RtlIsDosDeviceName_U(p.c_str()) > 0;
}
CATCH_FAIL_FAST();

bool isReservedBaseName(std::wstring_view name) noexcept {
    if (name.length() == 3) {
        if (CompareStringOrdinal(name.data(), 3, L"CON", 3, TRUE) == CSTR_EQUAL ||
            CompareStringOrdinal(name.data(), 3, L"PRN", 3, TRUE) == CSTR_EQUAL ||
            CompareStringOrdinal(name.data(), 3, L"AUX", 3, TRUE) == CSTR_EQUAL ||
            CompareStringOrdinal(name.data(), 3, L"NUL", 3, TRUE) == CSTR_EQUAL) {
            return true;
        }
    }
    if (name.length() >= 4) {
        if (CompareStringOrdinal(name.data(), static_cast<int>(name.length()), L"COM", 3, TRUE) == CSTR_EQUAL ||
            CompareStringOrdinal(name.data(), static_cast<int>(name.length()), L"LPT", 3, TRUE) == CSTR_EQUAL) {
            if (name.length() == 4 && L'1' <= name[3] && name[3] <= L'9') {
                return true;
            }
            // Superscript ¹, ², and ³ are considered numbers as well.
            switch (name[3]) {
            case L'\xb2':
            case L'\xb3':
            case L'\xb9':
                return true;
            }
            return false;
        }
    }

    // Passing CONIN$ or CONOUT$ to CreateFile opens a console handle.
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea#consoles
    //
    // While CONIN$ and CONOUT$ aren't documented as being files,
    // they behave the same as CON. For example, ./CONIN$ also opens the console input.
    if (name.length() == 6 && name[5] == '$' &&
        CompareStringOrdinal(name.data(), 6, L"CONIN$", 6, TRUE) == CSTR_EQUAL) {
        return true;
    }
    if (name.length() == 7 && name[6] == '$' &&
        CompareStringOrdinal(name.data(), 7, L"CONOUT$", 7, TRUE) == CSTR_EQUAL) {
        return true;
    }
    return false;
}

// IsAbs reports whether the path is absolute.
bool IsAbs(std::wstring_view path) noexcept try {
    auto l = volumeNameLen(path);
    if (l == 0) {
        return false;
    }
    // If the volume name starts with a double slash, this is an absolute path.
    if (IsPathSeparator(path[0]) && IsPathSeparator(path[1])) {
        return true;
    }
    path = path.substr(l);
    if (path.empty()) {
        return false;
    }
    return IsPathSeparator(path[0]);
}
CATCH_FAIL_FAST();

// volumeNameLen returns length of the leading volume name on Windows.
// It returns 0 elsewhere.
//
// See:
// https://learn.microsoft.com/en-us/dotnet/standard/io/file-path-formats
// https://googleprojectzero.blogspot.com/2016/02/the-definitive-guide-on-win32-to-nt.html
size_t volumeNameLen(std::wstring_view path) noexcept try {
    if (path.length() >= 2 && path[1] == ':') {
        // Path starts with a drive letter.
        //
        // Not all Windows functions necessarily enforce the requirement that
        // drive letters be in the set A-Z, and we don't try to here.
        //
        // We don't handle the case of a path starting with a non-ASCII character,
        // in which case the "drive letter" might be multiple bytes long.
        return 2;
    } else if (path.empty() || !IsPathSeparator(path[0])) {
        // Path does not have a volume component.
        return 0;
    } else if (pathHasPrefixFold(path, LR"(\\.\UNC)")) {
        // We're going to treat the UNC host and share as part of the volume
        // prefix for historical reasons, but this isn't really principled;
        // Windows's own GetFullPathName will happily remove the first
        // component of the path in this space, converting
        // \\.\unc\a\b\..\c into \\.\unc\a\c.
        return uncLen(path, wcslen(LR"(\\.\UNC\)"));
    } else if (
        pathHasPrefixFold(path, LR"(\\.)") || pathHasPrefixFold(path, LR"(\\?)") ||
        pathHasPrefixFold(path, LR"(\??)")) {
        // Path starts with \\.\, and is a Local Device path; or
        // path starts with \\?\ or \??\ and is a Root Local Device path.
        //
        // We treat the next component after the \\.\ prefix as
        // part of the volume name, which means Clean(`\\?\c:\`)
        // won't remove the trailing \. (See #64028.)
        if (path.length() == 3) {
            return 3; // exactly \\.
        }
        std::wstring_view _, rest;
        auto ok = cutPath(path.substr(4), _, rest);
        if (!ok) {
            return path.length();
        }
        return path.length() - rest.length() - 1;
    } else if (path.length() >= 2 && IsPathSeparator(path[1])) {
        // Path starts with \\, and is a UNC path.
        return uncLen(path, 2);
    }
    return 0;
}
CATCH_FAIL_FAST();

// pathHasPrefixFold tests whether the path s begins with prefix,
// ignoring case and treating all path separators as equivalent.
// If s is longer than prefix, then s[len(prefix)] must be a path separator.
bool pathHasPrefixFold(std::wstring_view s, std::wstring_view prefix) noexcept {
    if (s.length() < prefix.length()) {
        return false;
    }
    for (size_t i = 0; i < prefix.length(); i++) {
        if (IsPathSeparator(prefix[i])) {
            if (!IsPathSeparator(s[i])) {
                return false;
            }
        } else if (towupper(prefix[i]) != towupper(s[i])) {
            return false;
        }
    }
    if (s.length() > prefix.length() && !IsPathSeparator(s[prefix.length()])) {
        return false;
    }
    return true;
}

// uncLen returns the length of the volume prefix of a UNC path.
// prefixLen is the prefix prior to the start of the UNC host;
// for example, for "//host/share", the prefixLen is len("//")==2.
size_t uncLen(std::wstring_view path, size_t prefixLen) noexcept {
    auto count = 0;
    for (size_t i = prefixLen; i < path.length(); i++) {
        if (IsPathSeparator(path[i])) {
            count++;
            if (count == 2) {
                return i;
            }
        }
    }
    return path.length();
}

// cutPath slices path around the first path separator.
bool cutPath(std::wstring_view path, std::wstring_view &first, std::wstring_view &next) noexcept try {
    std::wstring_view _first, _next;
    for (size_t i = 0; i < path.length(); i++) {
        if (IsPathSeparator(path[i])) {
            _first = path.substr(0, i);
            _next = path.substr(i + 1);
            first = _first;
            next = _next;
            return true;
        }
    }
    _first = path;
    _next = std::wstring_view();
    first = _first;
    next = _next;
    return false;
}
CATCH_FAIL_FAST();

// NOTICE: must check non-deviation (!out.buf) separately
// postClean adjusts the results of Clean to avoid turning a relative path
// into an absolute or rooted one.
HRESULT postClean(std::wstring &out, size_t volLen) noexcept try {
    if (volLen != 0) {
        return S_OK;
    }
    // If a ':' appears in the path element at the start of a path,
    // insert a .\ at the beginning to avoid converting relative paths
    // like a/../c: into c:.
    for (auto c : out) {
        if (IsPathSeparator(c)) {
            break;
        }
        if (c == L':') {
            out.insert(0, L".\\");
            return S_OK;
        }
    }
    // If a path begins with \??\, insert a \. at the beginning
    // to avoid converting paths like \a\..\??\c:\x into \??\c:\x
    // (equivalent to c:\x).
    if (out.length() >= 3 && IsPathSeparator(out[0]) && out[1] == L'?' && out[2] == L'?') {
        out.insert(0, L"\\.");
    }
    return S_OK;
}
CATCH_RETURN();

} // namespace winrooted::detail::filepathlite
