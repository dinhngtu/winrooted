#pragma once

#include <string>

#include <Windows.h>

namespace winrooted::detail {

HRESULT readReparseLinkHandle(HANDLE h, std::wstring &link) noexcept;

}
