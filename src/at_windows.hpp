#pragma once

#include <string_view>

#include <Windows.h>

#include <wil/resource.h>

namespace winrooted::detail {

HRESULT
Openat(HANDLE dirfd, std::wstring_view name, ULONGLONG flag, DWORD perm, wil::unique_hfile &result) noexcept;
HRESULT Mkdirat(HANDLE dirfd, std::wstring_view name, DWORD mode) noexcept;
HRESULT Deleteat(HANDLE dirfd, std::wstring_view name, DWORD options) noexcept;
HRESULT Renameat(HANDLE olddirfd, std::wstring_view oldpath, HANDLE newdirfd, std::wstring_view newpath) noexcept;
HRESULT Linkat(HANDLE olddirfd, std::wstring_view oldpath, HANDLE newdirfd, std::wstring_view newpath) noexcept;

// SymlinkatFlags configure Symlinkat.
//
// Symbolic links have two properties: They may be directory or file links,
// and they may be absolute or relative.
//
// The Windows API defines flags describing these properties
// (SYMBOLIC_LINK_FLAG_DIRECTORY and SYMLINK_FLAG_RELATIVE),
// but the flags are passed to different system calls and
// do not have distinct values, so we define our own enumeration
// that permits expressing both.
enum SymlinkatFlags : ULONG {
    SYMLINKAT_DIRECTORY = 1 << 0,
    SYMLINKAT_RELATIVE = 1 << 1,
};

HRESULT
Symlinkat(std::wstring_view oldname, HANDLE newdirfd, std::wstring_view newname, SymlinkatFlags flags) noexcept;

} // namespace winrooted::detail
