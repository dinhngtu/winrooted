#pragma once

#include <string>
#include <string_view>

namespace winrooted::detail {

std::wstring joinPath(std::wstring_view dir, std::wstring_view name);

} // namespace winrooted::detail
