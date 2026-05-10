#pragma once

#include <string>

#include <Windows.h>

namespace winrooted::detail {

HRESULT FullPath(PCWSTR name, std::wstring &buf) noexcept;

}
