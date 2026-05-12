// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\internal\stringslite\strings.go

#include "stdafx.h"

#include <wil/result_macros.h>

namespace winrooted::detail::stringslite {

// NOTICE: Thanks to substr() being exceptionful, practically all of these functions had to be wrapped in a try block...

bool HasPrefix(std::wstring_view s, std::wstring_view prefix) noexcept try {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}
CATCH_FAIL_FAST();

bool HasSuffix(std::wstring_view s, std::wstring_view suffix) noexcept try {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}
CATCH_FAIL_FAST();

long IndexByte(std::wstring_view s, wchar_t c) noexcept {
    FAIL_FAST_IF(s.size() > (size_t)LONG_MAX);
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == c) {
            return static_cast<long>(i);
        }
    }
    return -1;
}

std::pair<std::wstring_view, bool> CutPrefix(std::wstring_view s, std::wstring_view prefix) noexcept try {
    if (!HasPrefix(s, prefix)) {
        return {s, false};
    }
    return {s.substr(prefix.size()), true};
}
CATCH_FAIL_FAST();

std::pair<std::wstring_view, bool> CutSuffix(std::wstring_view s, std::wstring_view suffix) noexcept try {
    if (!HasSuffix(s, suffix)) {
        return {s, false};
    }
    return {s.substr(0, s.size() - suffix.size()), true};
}
CATCH_FAIL_FAST();

std::wstring_view TrimPrefix(std::wstring_view s, std::wstring_view prefix) noexcept try {
    if (HasPrefix(s, prefix)) {
        return s.substr(prefix.size());
    }
    return s;
}
CATCH_FAIL_FAST();

std::wstring_view TrimSuffix(std::wstring_view s, std::wstring_view suffix) noexcept try {
    if (HasSuffix(s, suffix)) {
        return s.substr(0, s.size() - suffix.size());
    }
    return s;
}
CATCH_FAIL_FAST();

} // namespace winrooted::detail::stringslite
