#pragma once

#include <Windows.h>

EXTERN_C HRESULT WinrootedOpenRootAt(_Out_ HANDLE *result, _In_ HANDLE dirfd, _In_ PCWSTR dirName) WIN_NOEXCEPT;

EXTERN_C HRESULT WinrootedCreateFileAt(
    _Out_ HANDLE *result,
    _In_ HANDLE dirfd,
    _In_ PCWSTR fileName,
    _In_ DWORD desiredAccess,
    _In_ DWORD shareMode,
    _In_opt_ LPSECURITY_ATTRIBUTES securityAttributes,
    _In_ DWORD creationDisposition,
    _In_ DWORD flags,
    _In_ DWORD attributes) WIN_NOEXCEPT;

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

EXTERN_C HRESULT
WinrootedDoInRoot(_In_ HANDLE dirfd, _In_ PCWSTR path, _In_ WINROOTED_IN_ROOT_FUNC func, _Inout_opt_ PVOID context)
    WIN_NOEXCEPT;
