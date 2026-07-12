#include "pch.h"

#include <winrooted.h>

#include "linkutils.hpp"
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

EXTERN_C HRESULT WinrootedCreateFileAtCore(
    _Out_ HANDLE *result,
    _In_ HANDLE dirfd,
    _In_ PCWSTR fileName,
    _In_ DWORD desiredAccess,
    _In_ DWORD shareMode,
    _In_opt_ LPSECURITY_ATTRIBUTES securityAttributes,
    _In_ DWORD creationDisposition,
    _In_ DWORD flags,
    _In_ DWORD attributes,
    _In_ ULONG ntOptions,
    _When_(
        return == __HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED),
               _Post_valid_ _At_(*link, _Post_notnull_))
        PWSTR *link) WIN_NOEXCEPT try {
    auto opened = winrooted::OpenAtCore(
        dirfd,
        fileName,
        desiredAccess,
        shareMode,
        securityAttributes,
        creationDisposition,
        flags,
        attributes,
        ntOptions);
    if (std::holds_alternative<std::wstring>(opened)) {
        auto newLink = std::move(std::get<std::wstring>(opened));
        auto dupLink = _wcsdup(newLink.c_str());
        THROW_IF_NULL_ALLOC(dupLink);
        *link = dupLink;
        *result = nullptr;
        return HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED);
    } else {
        auto newFile = std::move(std::get<wil::unique_hfile>(opened));
        *link = nullptr;
        *result = newFile.release();
        return S_OK;
    }
}
CATCH_RETURN();

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen) EXTERN_C
    void *WinrootedMakeFileRenameInformation(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) WIN_NOEXCEPT {
    return winrooted::MakeFileRenameInformation(fileName, resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen) EXTERN_C
    void *WinrootedMakeFileRenameInformationEx(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) WIN_NOEXCEPT {
    return winrooted::MakeFileRenameInformationEx(fileName, resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen) EXTERN_C
    void *WinrootedMakeFileLinkInformation(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) WIN_NOEXCEPT {
    return winrooted::MakeFileLinkInformation(fileName, resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen) EXTERN_C
    void *WinrootedMakeReparseDataBufferMountPoint(
        _In_ PCWSTR substituteName,
        _In_ PCWSTR printName,
        _Out_ PULONG resultLen) WIN_NOEXCEPT {
    return winrooted::MakeReparseDataBufferMountPoint(
        substituteName,
        printName,
        resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen) EXTERN_C
    void *WinrootedMakeReparseDataBufferSymbolicLink(
        _In_ PCWSTR substituteName,
        _In_ PCWSTR printName,
        _Out_ PULONG resultLen) WIN_NOEXCEPT {
    return winrooted::MakeReparseDataBufferSymbolicLink(
        substituteName,
        printName,
        resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen) EXTERN_C
    void *WinrootedMakeReparseDataBufferLxSymlink(
        _In_ PCSTR target,
        _Out_ PULONG resultLen) WIN_NOEXCEPT {
    return winrooted::MakeReparseDataBufferLxSymlink(target, resultLen);
}
