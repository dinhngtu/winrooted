#include "pch.h"

namespace winrooted {

template <typename T>
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    T *MakeWithFileName(_In_ PCWSTR fileName, _Out_ PULONG resultLen) noexcept {
    if (!fileName) {
        *resultLen = 0;
        return nullptr;
    }
    auto fileNameLength = wcslen(fileName);
    if (fileNameLength >
        (ULONG_MAX - UFIELD_OFFSET(T, FileName)) / sizeof(wchar_t)) {
        *resultLen = 0;
        return nullptr;
    }
    auto fileNameSize = fileNameLength * sizeof(wchar_t);
    auto _resultLen = UFIELD_OFFSET(T, FileName) + fileNameSize;
    auto p = static_cast<T *>(calloc(1, _resultLen));
    if (!p) {
        *resultLen = 0;
        return nullptr;
    }
    p->FileNameLength = static_cast<ULONG>(fileNameSize);
    memcpy(&p->FileName[0], fileName, fileNameSize);
    *resultLen = static_cast<ULONG>(_resultLen);
    return p;
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    FILE_RENAME_INFORMATION *MakeFileRenameInformation(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) noexcept {
    return MakeWithFileName<FILE_RENAME_INFORMATION>(fileName, resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    FILE_RENAME_INFORMATION_EX *MakeFileRenameInformationEx(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) noexcept {
    return MakeWithFileName<FILE_RENAME_INFORMATION_EX>(fileName, resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    FILE_LINK_INFORMATION *MakeFileLinkInformation(
        _In_ PCWSTR fileName,
        _In_ PULONG resultLen) noexcept {
    return MakeWithFileName<FILE_LINK_INFORMATION>(fileName, resultLen);
}

struct RDBMountPoint {
    constexpr ULONG ReparseTag() const {
        return IO_REPARSE_TAG_MOUNT_POINT;
    }

    constexpr DWORD PathBufferOffset() const {
        return UFIELD_OFFSET(
            REPARSE_DATA_BUFFER,
            MountPointReparseBuffer.PathBuffer[0]);
    }

    constexpr PWCHAR PathBuffer(PREPARSE_DATA_BUFFER rdb) const {
        return &rdb->MountPointReparseBuffer.PathBuffer[0];
    }

    constexpr void SetSubstituteName(
        PREPARSE_DATA_BUFFER rdb,
        USHORT offset,
        USHORT length) const {
        rdb->MountPointReparseBuffer.SubstituteNameOffset = offset;
        rdb->MountPointReparseBuffer.SubstituteNameLength = length;
    }

    constexpr void
    SetPrintName(PREPARSE_DATA_BUFFER rdb, USHORT offset, USHORT length) const {
        rdb->MountPointReparseBuffer.PrintNameOffset = offset;
        rdb->MountPointReparseBuffer.PrintNameLength = length;
    }
};

struct RDBSymbolicLink {
    constexpr ULONG ReparseTag() const {
        return IO_REPARSE_TAG_SYMLINK;
    }

    constexpr DWORD PathBufferOffset() const {
        return UFIELD_OFFSET(
            REPARSE_DATA_BUFFER,
            SymbolicLinkReparseBuffer.PathBuffer[0]);
    }

    constexpr PWCHAR PathBuffer(PREPARSE_DATA_BUFFER rdb) const {
        return &rdb->SymbolicLinkReparseBuffer.PathBuffer[0];
    }

    constexpr void SetSubstituteName(
        PREPARSE_DATA_BUFFER rdb,
        USHORT offset,
        USHORT length) const {
        rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset = offset;
        rdb->SymbolicLinkReparseBuffer.SubstituteNameLength = length;
    }

    constexpr void
    SetPrintName(PREPARSE_DATA_BUFFER rdb, USHORT offset, USHORT length) const {
        rdb->SymbolicLinkReparseBuffer.PrintNameOffset = offset;
        rdb->SymbolicLinkReparseBuffer.PrintNameLength = length;
    }
};

template <typename T>
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    REPARSE_DATA_BUFFER *MakeReparseDataBufferSubstituteNameAndPrintName(
        _In_ const T Accessor,
        _In_ PCWSTR substituteName,
        _In_ PCWSTR printName,
        _Out_ PULONG resultLen) noexcept {
    auto substituteNameLength = substituteName ? wcslen(substituteName) : 0;
    auto printNameLength = printName ? wcslen(printName) : 0;
    if (substituteNameLength > USHORT_MAX / sizeof(wchar_t) ||
        printNameLength > USHORT_MAX / sizeof(wchar_t)) {
        *resultLen = 0;
        return nullptr;
    }

    auto substituteNameSize = substituteNameLength * sizeof(wchar_t);
    auto printNameSize = printNameLength * sizeof(wchar_t);
    const auto pathBufferSubOffset = Accessor.PathBufferOffset() -
        UFIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer);
    auto reparseDataLength =
        pathBufferSubOffset + substituteNameSize + printNameSize;
    if (reparseDataLength > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) {
        *resultLen = 0;
        return nullptr;
    }
    auto _resultLen = UFIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) +
        reparseDataLength;
    if (_resultLen > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) {
        *resultLen = 0;
        return nullptr;
    }

    auto p = static_cast<PREPARSE_DATA_BUFFER>(calloc(1, _resultLen));
    if (!p) {
        *resultLen = 0;
        return nullptr;
    }
    p->ReparseTag = Accessor.ReparseTag();
    p->ReparseDataLength = static_cast<USHORT>(reparseDataLength);
    Accessor.SetSubstituteName(
        p,
        static_cast<USHORT>(pathBufferSubOffset),
        static_cast<USHORT>(substituteNameSize));
    Accessor.SetPrintName(
        p,
        static_cast<USHORT>(pathBufferSubOffset + substituteNameSize),
        static_cast<USHORT>(printNameSize));
    if (substituteName) {
        memcpy(Accessor.PathBuffer(p), substituteName, substituteNameSize);
    }
    if (printName) {
        memcpy(
            Accessor.PathBuffer(p) + substituteNameLength,
            printName,
            printNameSize);
    }
    *resultLen = static_cast<ULONG>(_resultLen);
    return p;
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    REPARSE_DATA_BUFFER *MakeReparseDataBufferMountPoint(
        _In_ PCWSTR substituteName,
        _In_ PCWSTR printName,
        _Out_ PULONG resultLen) noexcept {
    return MakeReparseDataBufferSubstituteNameAndPrintName(
        RDBMountPoint{},
        substituteName,
        printName,
        resultLen);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    REPARSE_DATA_BUFFER *MakeReparseDataBufferSymbolicLink(
        _In_ PCWSTR substituteName,
        _In_ PCWSTR printName,
        _Out_ PULONG resultLen) noexcept {
    return MakeReparseDataBufferSubstituteNameAndPrintName(
        RDBSymbolicLink{},
        substituteName,
        printName,
        resultLen);
}

constexpr ULONG WINROOTED_IO_REPARSE_TAG_LX_SYMLINK = 0xA000001DU;

struct LxSymlinkReparseBuffer {
    ULONG Version;
    UCHAR Target[1];
};

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(*resultLen)
    REPARSE_DATA_BUFFER *MakeReparseDataBufferLxSymlink(
        _In_ PCSTR target,
        _Out_ PULONG resultLen) noexcept {
    auto targetLength = target ? strlen(target) : 0;
    if (targetLength > USHORT_MAX / sizeof(wchar_t)) {
        *resultLen = 0;
        return nullptr;
    }

    auto targetSize = targetLength * sizeof(wchar_t);
    const auto pathBufferSubOffset =
        UFIELD_OFFSET(LxSymlinkReparseBuffer, Target[0]);
    auto reparseDataLength = pathBufferSubOffset + targetSize;
    if (reparseDataLength > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) {
        *resultLen = 0;
        return nullptr;
    }
    auto _resultLen = UFIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) +
        reparseDataLength;
    if (_resultLen > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) {
        *resultLen = 0;
        return nullptr;
    }

    auto p = static_cast<PREPARSE_DATA_BUFFER>(calloc(1, _resultLen));
    if (!p) {
        *resultLen = 0;
        return nullptr;
    }
    p->ReparseTag = WINROOTED_IO_REPARSE_TAG_LX_SYMLINK;
    p->ReparseDataLength = static_cast<USHORT>(reparseDataLength);

    auto lx = reinterpret_cast<LxSymlinkReparseBuffer *>(
        &p->GenericReparseBuffer.DataBuffer[0]);
    lx->Version = 2;
    if (target) {
        memcpy(&lx->Target[0], target, targetSize);
    }
    *resultLen = static_cast<ULONG>(_resultLen);
    return p;
}

} // namespace winrooted
