#pragma once

#include <Mile.Internal.h>

#include <string>

#include <Windows.h>

namespace winrooted {

struct ObjectAttributes : public OBJECT_ATTRIBUTES {
    std::wstring StoredName;
    UNICODE_STRING StoredString;

    ObjectAttributes(std::wstring_view name, ULONG attributes, HANDLE root = nullptr);
    ObjectAttributes(const ObjectAttributes &) = delete;
    ObjectAttributes &operator=(const ObjectAttributes &) = delete;
    ObjectAttributes(ObjectAttributes &&other) = delete;
    ObjectAttributes &operator=(ObjectAttributes &&other) = delete;
    ~ObjectAttributes() {}
};

} // namespace winrooted
