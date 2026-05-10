#include "stdafx.h"

#include "winrooted_internal.hpp"

#include <wil/result_macros.h>

namespace winrooted::detail {

template <typename T>
wil::unique_process_heap_ptr<T> make_with_file_name(std::wstring_view fileName, PULONG resultLen) noexcept {
    if (fileName.length() > ULONG_MAX / sizeof(wchar_t)) {
        *resultLen = 0;
        return {};
    }
    auto fileNameLength = fileName.length() * sizeof(wchar_t);
    auto _resultLen = offsetof(T, FileName) + fileNameLength;
    auto p = static_cast<T *>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, _resultLen));
    if (!p) {
        *resultLen = 0;
        return {};
    }
    p->FileNameLength = static_cast<ULONG>(fileNameLength);
    memcpy(&p->FileName[0], fileName.data(), fileNameLength);
    *resultLen = static_cast<ULONG>(_resultLen);
    return wil::unique_process_heap_ptr<T>(p);
}

wil::unique_process_heap_ptr<FILE_RENAME_INFORMATION>
make_file_rename_information(std::wstring_view fileName, PULONG resultLen) noexcept {
    return make_with_file_name<FILE_RENAME_INFORMATION>(fileName, resultLen);
}

wil::unique_process_heap_ptr<FILE_RENAME_INFORMATION_EX>
make_file_rename_information_ex(std::wstring_view fileName, PULONG resultLen) noexcept {
    return make_with_file_name<FILE_RENAME_INFORMATION_EX>(fileName, resultLen);
}

wil::unique_process_heap_ptr<FILE_LINK_INFORMATION>
make_file_link_information(std::wstring_view fileName, PULONG resultLen) noexcept {
    return make_with_file_name<FILE_LINK_INFORMATION>(fileName, resultLen);
}

wil::unique_process_heap_ptr<REPARSE_DATA_BUFFER> make_reparse_data_buffer_symlink(
    std::wstring_view substituteName,
    std::wstring_view printName,
    PULONG resultLen) noexcept {
    if (substituteName.length() > USHORT_MAX / sizeof(wchar_t) || printName.length() > USHORT_MAX / sizeof(wchar_t)) {
        *resultLen = 0;
        return {};
    }
    auto substituteNameLength = substituteName.length() * sizeof(wchar_t);
    auto printNameLength = printName.length() * sizeof(wchar_t);
    const auto pathBufferSubOffset = (uintptr_t)(&((PREPARSE_DATA_BUFFER)0)->SymbolicLinkReparseBuffer.PathBuffer[0]) -
        offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer);
    auto reparseDataLength = pathBufferSubOffset + substituteNameLength + printNameLength;
    if (reparseDataLength > USHORT_MAX) {
        *resultLen = 0;
        return {};
    }
    auto _resultLen = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer) + reparseDataLength;

    auto p = static_cast<PREPARSE_DATA_BUFFER>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, _resultLen));
    if (!p) {
        *resultLen = 0;
        return {};
    }
    p->ReparseDataLength = static_cast<USHORT>(reparseDataLength);
    p->SymbolicLinkReparseBuffer = {
        .SubstituteNameOffset = static_cast<USHORT>(pathBufferSubOffset),
        .SubstituteNameLength = static_cast<USHORT>(substituteNameLength),
        .PrintNameOffset = static_cast<USHORT>(pathBufferSubOffset + substituteNameLength),
        .PrintNameLength = static_cast<USHORT>(printNameLength),
    };
    if (!substituteName.empty()) {
        memcpy(&p->SymbolicLinkReparseBuffer.PathBuffer[0], substituteName.data(), substituteNameLength);
    }
    if (!printName.empty()) {
        memcpy(&p->SymbolicLinkReparseBuffer.PathBuffer[substituteName.length()], printName.data(), printNameLength);
    }
    *resultLen = static_cast<ULONG>(_resultLen);
    return wil::unique_process_heap_ptr<REPARSE_DATA_BUFFER>(p);
}

} // namespace winrooted::detail
