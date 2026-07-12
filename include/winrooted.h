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
