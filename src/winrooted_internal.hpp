#pragma once

#include <Mile.Internal.h>

#include <string_view>

#include <wil/resource.h>

namespace winrooted::detail {

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN10_RS1)
#define FILE_DISPOSITION_DO_NOT_DELETE 0x00000000
#define FILE_DISPOSITION_DELETE 0x00000001
#define FILE_DISPOSITION_POSIX_SEMANTICS 0x00000002
#define FILE_DISPOSITION_FORCE_IMAGE_SECTION_CHECK 0x00000004
#define FILE_DISPOSITION_ON_CLOSE 0x00000008
#if (NTDDI_VERSION >= NTDDI_WIN10_RS5)
#define FILE_DISPOSITION_IGNORE_READONLY_ATTRIBUTE 0x00000010
#endif

typedef struct _FILE_DISPOSITION_INFORMATION_EX {
    ULONG Flags;
} FILE_DISPOSITION_INFORMATION_EX, *PFILE_DISPOSITION_INFORMATION_EX;
#endif

wil::unique_process_heap_ptr<FILE_RENAME_INFORMATION>
make_file_rename_information(std::wstring_view fileName, PULONG resultLen) noexcept;
wil::unique_process_heap_ptr<FILE_RENAME_INFORMATION_EX>
make_file_rename_information_ex(std::wstring_view fileName, PULONG resultLen) noexcept;
wil::unique_process_heap_ptr<FILE_LINK_INFORMATION>
make_file_link_information(std::wstring_view fileName, PULONG resultLen) noexcept;
wil::unique_process_heap_ptr<REPARSE_DATA_BUFFER> make_reparse_data_buffer_symlink(
    std::wstring_view substituteName,
    std::wstring_view printName,
    PULONG resultLen) noexcept;

} // namespace winrooted::detail
