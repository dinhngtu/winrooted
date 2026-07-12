#pragma once

#include <Mile.Internal.h>

#include <Windows.h>

namespace winrooted {

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    FILE_RENAME_INFORMATION *MakeFileRenameInformation(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) noexcept;

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    FILE_RENAME_INFORMATION_EX *MakeFileRenameInformationEx(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) noexcept;

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    FILE_LINK_INFORMATION *MakeFileLinkInformation(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) noexcept;

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    REPARSE_DATA_BUFFER *MakeReparseDataBufferMountPoint(
        _In_ PCWSTR substituteName,
        _In_ PCWSTR printName,
        _Out_ PULONG resultLen) noexcept;

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    REPARSE_DATA_BUFFER *MakeReparseDataBufferSymbolicLink(
        _In_ PCWSTR substituteName,
        _In_ PCWSTR printName,
        _Out_ PULONG resultLen) noexcept;

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    REPARSE_DATA_BUFFER *MakeReparseDataBufferLxSymlink(
        _In_ PCSTR target,
        _Out_ PULONG resultLen) noexcept;

} // namespace winrooted
