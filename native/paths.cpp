// Copyright 2009-2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "pch.h"
#include "paths.hpp"

#include <wil/result_macros.h>

namespace winrooted::native {

std::wstring FullPath(PCWSTR name) {
    size_t n = 100;
    std::wstring buf;
    while (1) {
        buf = std::wstring(n, L'\0');
        n = GetFullPathNameW(name, static_cast<DWORD>(buf.length()), buf.data(), nullptr);
        THROW_LAST_ERROR_IF(!n);
        if (n <= buf.size()) {
            buf.resize(n);
            return buf;
        }
    }
}

static bool IsReservedBaseName(std::wstring_view name) {
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

static bool IsReservedName(std::wstring_view name) {
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
    if (!IsReservedBaseName(base)) {
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

static std::tuple<std::wstring_view, std::wstring_view, bool> CutPath(std::wstring_view path) {
    std::wstring_view _first, _next;
    for (size_t i = 0; i < path.length(); i++) {
        if (IsPathSeparator(path[i])) {
            return {path.substr(0, i), path.substr(i + 1), true};
        }
    }
    return {path, {}, false};
}

static bool PathHasPrefixFold(std::wstring_view s, std::wstring_view prefix) {
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
static size_t UncLen(std::wstring_view path, size_t prefixLen) {
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

static size_t VolumeNameLen(std::wstring_view path) {
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
    } else if (PathHasPrefixFold(path, LR"(\\.\UNC)")) {
        // We're going to treat the UNC host and share as part of the volume
        // prefix for historical reasons, but this isn't really principled;
        // Windows's own GetFullPathName will happily remove the first
        // component of the path in this space, converting
        // \\.\unc\a\b\..\c into \\.\unc\a\c.
        return UncLen(path, wcslen(LR"(\\.\UNC\)"));
    } else if (
        PathHasPrefixFold(path, LR"(\\.)") || PathHasPrefixFold(path, LR"(\\?)") ||
        PathHasPrefixFold(path, LR"(\??)")) {
        // Path starts with \\.\, and is a Local Device path; or
        // path starts with \\?\ or \??\ and is a Root Local Device path.
        //
        // We treat the next component after the \\.\ prefix as
        // part of the volume name, which means Clean(`\\?\c:\`)
        // won't remove the trailing \. (See #64028.)
        if (path.length() == 3) {
            return 3; // exactly \\.
        }
        auto [_, rest, ok] = CutPath(path.substr(4));
        if (!ok) {
            return path.length();
        }
        return path.length() - rest.length() - 1;
    } else if (path.length() >= 2 && IsPathSeparator(path[1])) {
        // Path starts with \\, and is a UNC path.
        return UncLen(path, 2);
    }
    return 0;
}

static void PostClean(std::wstring &out, size_t volLen) {
    if (volLen != 0) {
        return;
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
            return;
        }
    }
    // If a path begins with \??\, insert a \. at the beginning
    // to avoid converting paths like \a\..\??\c:\x into \??\c:\x
    // (equivalent to c:\x).
    if (out.length() >= 3 && IsPathSeparator(out[0]) && out[1] == L'?' && out[2] == L'?') {
        out.insert(0, L"\\.");
    }
}

static constexpr wchar_t Separator = L'\\';

static std::wstring Clean(std::wstring_view path) {
    auto originalPath = path;
    auto volLen = VolumeNameLen(path);
    path = path.substr(volLen);
    if (path.empty()) {
        if (volLen > 1 && IsPathSeparator(originalPath[0]) && IsPathSeparator(originalPath[1])) {
            // should be UNC
            auto result = std::wstring(originalPath);
            // NOTICE: FromSlash inlined
            std::replace(result.begin(), result.end(), L'/', Separator);
            return result;
        }
        return std::wstring(originalPath) + L".";
    }
    auto rooted = IsPathSeparator(path[0]);

    // NOTICE: lazybuf was not ported

    // Invariants:
    //	reading from path; r is index of next byte to process.
    //	writing to buf; w is index of next byte to write.
    //	dotdot is index in buf where .. must stop, either because
    //		it is the leading slash or it is a leading ../../.. prefix.
    auto n = path.length();
    std::wstring out;
    out.reserve(volLen + n + 2);
    size_t r = 0;
    size_t dotdot = 0;
    if (rooted) {
        out += Separator;
        r = dotdot = 1;
    }

    while (r < n) {
        if (IsPathSeparator(path[r])) {
            // empty path element
            r++;
        } else if (path[r] == L'.' && (r + 1 == n || IsPathSeparator(path[r + 1]))) {
            r++;
        } else if (path[r] == L'.' && path[r + 1] == L'.' && (r + 2 == n || IsPathSeparator(path[r + 2]))) {
            // .. element: remove to last separator
            r += 2;
            if (out.length() > dotdot) {
                // can backtrack
                out.pop_back();
                while (out.length() > dotdot && !IsPathSeparator(out.back())) {
                    out.pop_back();
                }
            } else if (!rooted) {
                // cannot backtrack, but not rooted, so append .. element.
                if (!out.empty()) {
                    out += Separator;
                }
                out += L"..";
                dotdot = out.length();
            }
        } else {
            // real path element.
            // add slash if needed
            if ((rooted && out.length() != 1) || (!rooted && !out.empty())) {
                out += Separator;
            }
            // copy element
            for (; r < n && !IsPathSeparator(path[r]); r++) {
                out += path[r];
            }
        }
    }

    // Turn empty string into "."
    if (out.empty()) {
        out += L'.';
    }

    // NOTICE: check here instead of further in postClean
    if (out != path) {
        // NOTICE: postClean changed
        PostClean(out, volLen); // avoid creating absolute paths on Windows
    }
    // NOTICE: FromSlash inlined
    std::replace(out.begin(), out.end(), L'/', Separator);
    return out;
}

bool IsLocal(std::wstring_view path) {
    if (path.empty()) {
        return false;
    }
    if (IsPathSeparator(path[0])) {
        return false;
    }
    if (path.find(L':') != std::wstring_view::npos) {
        // Colons are only valid when marking a drive letter ("C:foo").
        // Rejecting any path with a colon is conservative but safe.
        return false;
    }
    bool hasDots = false; // contains . or .. path elements
    for (auto p = path; !p.empty();) {
        std::wstring_view part;
        std::tie(p, part, std::ignore) = CutPath(p);
        if (part == L"." || part == L"..") {
            hasDots = true;
        }
        if (IsReservedName(part)) {
            return false;
        }
    }
    if (hasDots) {
        std::wstring cleaned = Clean(path);
        path = std::move(cleaned);
    }
    if (path == L".." || HasPrefix(path, L"..\\")) {
        return false;
    }
    return true;
}

} // namespace winrooted::native
