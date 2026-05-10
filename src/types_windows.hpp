#pragma once

#include <Mile.Internal.h>

#include <string>

#include <Windows.h>

namespace winrooted::detail {

struct ObjectAttributes : public OBJECT_ATTRIBUTES {
    std::wstring StoredName;
    UNICODE_STRING StoredString;

    ObjectAttributes() {
        memset(static_cast<POBJECT_ATTRIBUTES>(this), 0, sizeof(OBJECT_ATTRIBUTES));
        this->Length = sizeof(OBJECT_ATTRIBUTES);
    }
    ObjectAttributes(const ObjectAttributes &) = delete;
    ObjectAttributes &operator=(const ObjectAttributes &) = delete;
    ObjectAttributes(ObjectAttributes &&other) = delete;
    ObjectAttributes &operator=(ObjectAttributes &&other) = delete;
    ~ObjectAttributes() {}

    HRESULT init(HANDLE root, std::wstring_view name) noexcept;
};

} // namespace winrooted::detail
