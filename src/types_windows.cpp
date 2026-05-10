// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\internal\syscall\windows\types_windows.go

#include "stdafx.h"

#include "types_windows.hpp"

#include <wil/result_macros.h>

namespace winrooted::detail {

HRESULT ObjectAttributes::init(HANDLE root, std::wstring_view name) noexcept {
    try {
        this->StoredName = name;
        if (this->StoredName == L".") {
            this->StoredName.clear();
        }
        this->StoredString = UNICODE_STRING{
            .Length = static_cast<USHORT>(wcslen(this->StoredName.data())),
            .MaximumLength = static_cast<USHORT>(this->StoredName.length()),
            .Buffer = this->StoredName.data(),
        };
        this->ObjectName = &this->StoredString;
        this->RootDirectory = nullptr;
        if (root != INVALID_HANDLE_VALUE) {
            this->RootDirectory = root;
        }
        this->Length = sizeof(OBJECT_ATTRIBUTES);
        return S_OK;
    }
    CATCH_RETURN();
}

} // namespace winrooted::detail
