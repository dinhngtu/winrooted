#include "pch.h"

#include <winrooted.h>

#include "openat.hpp"

#include <wil/resource.h>

EXTERN_C HRESULT WinrootedOpenRootAt(
    _Out_ HANDLE *result,
    _In_ HANDLE dirfd,
    _In_ PCWSTR dirName) WIN_NOEXCEPT try {
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

static std::variant<HRESULT, std::wstring> WinrootedDoInRootOne(
    _In_ HANDLE parent,
    _In_ std::wstring_view name,
    _In_ WINROOTED_IN_ROOT_FUNC func,
    _Inout_opt_ PVOID context) WIN_NOEXCEPT try {
    std::wstring namestr(name);
    PWSTR newlink = nullptr;
    HRESULT hr;

    try {
        hr = func(parent, namestr.c_str(), context, &newlink);
    }
    CATCH_FAIL_FAST_MSG("WINROOTED_IN_ROOT_FUNC threw an unhandled exception");

    if (hr == HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED)) {
        FAIL_FAST_IF_NULL_MSG(
            newlink,
            "WINROOTED_IN_ROOT_FUNC returned a null link with "
            "ERROR_REPARSE_POINT_ENCOUNTERED");
        auto newlink_free = wil::scope_exit([=]() { free(newlink); });
        return std::wstring(newlink);
    } else {
        return hr;
    }
}
CATCH_RETURN();

EXTERN_C HRESULT WinrootedDoInRoot(
    _In_ HANDLE dirfd,
    _In_ PCWSTR path,
    _In_ WINROOTED_IN_ROOT_FUNC func,
    _Inout_opt_ PVOID context) WIN_NOEXCEPT try {
    return winrooted::DoInRoot<HRESULT>( //
        dirfd,
        path,
        nullptr,
        [=](HANDLE parent, std::wstring_view name) {
            return WinrootedDoInRootOne(parent, name, func, context);
        });
}
CATCH_RETURN();
