// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// From src\internal\syscall\windows\types_windows.go

#include "pch.h"

#include "objattrs.hpp"

#include <wil/result_macros.h>

namespace winrooted {

ObjectAttributes::ObjectAttributes(
    std::wstring_view name,
    ULONG attributes,
    HANDLE root) {
    memset(static_cast<POBJECT_ATTRIBUTES>(this), 0, sizeof(OBJECT_ATTRIBUTES));
    this->Length = sizeof(OBJECT_ATTRIBUTES);
    if (root != INVALID_HANDLE_VALUE) {
        this->RootDirectory = root;
    }
    this->StoredName = name;
    if (this->StoredName == L".") {
        this->StoredName.clear();
    }
    this->StoredString = UNICODE_STRING{
        .Length = static_cast<USHORT>(this->StoredName.length()),
        .MaximumLength = static_cast<USHORT>(this->StoredName.capacity()),
        .Buffer = this->StoredName.data(),
    };
    this->ObjectName = &this->StoredString;
    this->Attributes = attributes;
}

} // namespace winrooted
