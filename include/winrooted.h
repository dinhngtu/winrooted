#pragma once

#include <Windows.h>

EXTERN_C HRESULT WinrootedOpenRootAt(HANDLE *result, HANDLE dirfd, PCWSTR dirName) WIN_NOEXCEPT;

EXTERN_C HRESULT WinrootedCreateFileAt(
    HANDLE *result,
    HANDLE dirfd,
    PCWSTR fileName,
    DWORD desiredAccess,
    DWORD shareMode,
    LPSECURITY_ATTRIBUTES securityAttributes,
    DWORD creationDisposition,
    DWORD flags,
    DWORD attributes) WIN_NOEXCEPT;
