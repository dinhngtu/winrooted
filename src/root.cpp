#include "pch.h"

#include <winrooted.h>

#include "openat.hpp"

#include <wil/resource.h>

EXTERN_C HRESULT WinrootedOpenRootAt(HANDLE *result, HANDLE dirfd, PCWSTR dirName) WIN_NOEXCEPT try {
    *result = winrooted::DoInRoot<wil::unique_hfile>( //
        dirfd,
        dirName,
        nullptr,
        winrooted::OpenDirAt).release();
    return S_OK;
}
CATCH_RETURN();

EXTERN_C HRESULT WinrootedCreateFileAt(
    HANDLE *result,
    HANDLE dirfd,
    PCWSTR fileName,
    DWORD desiredAccess,
    DWORD shareMode,
    LPSECURITY_ATTRIBUTES securityAttributes,
    DWORD creationDisposition,
    DWORD flags,
    DWORD attributes) WIN_NOEXCEPT try {
    *result = winrooted::DoInRoot<wil::unique_hfile>( //
        dirfd,
        fileName,
        nullptr,
        [=](HANDLE parent, std::wstring_view name) {
            return winrooted::OpenAtCore(
                parent,
                name,
                desiredAccess,
                shareMode,
                securityAttributes,
                creationDisposition,
                flags,
                attributes,
                0);
        }).release();
    return S_OK;
}
CATCH_RETURN();
