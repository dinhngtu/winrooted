#include "pch.h"

#include <winrooted.h>

#include "openat.hpp"

#include <wil/resource.h>

EXTERN_C HRESULT WinrootedOpenRootAt(_Out_ HANDLE *result, _In_ HANDLE dirfd, _In_ PCWSTR dirName) WIN_NOEXCEPT try {
    *result = winrooted::DoInRoot<wil::unique_hfile>( //
        dirfd,
        dirName,
        nullptr,
        winrooted::OpenDirAt).release();
    return S_OK;
}
CATCH_RETURN();

EXTERN_C HRESULT WinrootedCreateFileAt(
    _Out_ HANDLE *result,
    _In_ HANDLE dirfd,
    _In_ PCWSTR fileName,
    _In_ DWORD desiredAccess,
    _In_ DWORD shareMode,
    _In_opt_ LPSECURITY_ATTRIBUTES securityAttributes,
    _In_ DWORD creationDisposition,
    _In_ DWORD flags,
    _In_ DWORD attributes) WIN_NOEXCEPT try {
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
