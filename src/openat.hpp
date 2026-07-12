// Copyright 2024 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#include "paths.hpp"

#include <functional>
#include <optional>
#include <string_view>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <Windows.h>

#include <wil/resource.h>
#include <wil/result_macros.h>

namespace winrooted {

std::variant<wil::unique_hfile, std::wstring> OpenAtCore(
    HANDLE dirfd,
    std::wstring_view fileName,
    DWORD desiredAccess,
    DWORD shareMode,
    LPSECURITY_ATTRIBUTES securityAttributes,
    DWORD creationDisposition,
    DWORD flags,
    DWORD attributes,
    ULONG ntOptions);
std::variant<wil::unique_hfile, std::wstring>
OpenDirAt(HANDLE parent, std::wstring_view name);

template <typename Result>
using DoInRootFunc = std::function<
    std::variant<Result, std::wstring>(HANDLE parent, std::wstring_view name)>;
using OpenDirFunc = DoInRootFunc<wil::unique_hfile>;

template <typename Result>
static Result DoInRoot(
    HANDLE rootfd,
    std::wstring_view name,
    OpenDirFunc &&openDirFunc,
    DoInRootFunc<Result> &&f) {
    constexpr auto EmptyIt = std::vector<std::wstring>::iterator{};

    auto [parts, suffixSep] =
        SplitPathInRoot(name, EmptyIt, EmptyIt, EmptyIt, EmptyIt);
    if (!openDirFunc) {
        openDirFunc = OpenDirAt;
    }

    auto dirfd = rootfd;
    auto defer_dirfd = wil::scope_exit([&]() {
        if (dirfd != rootfd) {
            CloseHandle(dirfd);
        }
    });

    constexpr int rootMaxSymlinks = 8;
    constexpr int maxSteps = 255;
    constexpr int maxRestarts = 8;
    size_t i = 0;
    int steps = 0, restarts = 0, symlinks = 0;

    while (true) {
        steps++;
        THROW_WIN32_IF(
            ERROR_FILENAME_EXCED_RANGE,
            steps > maxSteps && restarts > maxRestarts);

        const auto &comp = parts[i];

        if (comp == L"..") {
            // Resolve one or more parent ("..") path components.
            //
            // Rewrite the original path,
            // removing the elements eliminated by ".." components,
            // and start over from the beginning.
            restarts++;
            auto end = i + 1;
            while (end < parts.size() && parts[end] == L"..") {
                end++;
            }
            auto count = end - i;
            THROW_WIN32_IF(ERROR_ACCESS_DENIED, count > i);
            parts.erase(parts.begin() + i - count, parts.begin() + end);
            if (parts.empty()) {
                parts.push_back(L".");
            }
            i = 0;
            if (dirfd != rootfd) {
                CloseHandle(dirfd);
            }
            dirfd = rootfd;
            continue;
        }

        std::optional<std::wstring> link;
        if (i == parts.size() - 1) {
            // This is the last path element.
            // Call f to decide what to do with it.
            // If f returns errSymlink, this element is a symlink
            // which should be followed.
            // suffixSep contains any trailing separator characters
            // which we rejoin to the final part at this time.
            std::wstring lastComp = parts[i] + suffixSep;
            auto result = f(dirfd, lastComp.c_str());
            if (std::holds_alternative<std::wstring>(result)) {
                link = std::move(std::get<std::wstring>(result));
            } else {
                return std::move(std::get<Result>(result));
            }
        } else {
            auto result = openDirFunc(dirfd, parts[i].c_str());
            if (std::holds_alternative<std::wstring>(result)) {
                link = std::move(std::get<std::wstring>(result));
            } else {
                auto h = std::move(std::get<wil::unique_hfile>(result));
                if (dirfd != rootfd) {
                    CloseHandle(dirfd);
                }
                dirfd = h.release();
            }
        }

        if (link) {
            THROW_HR_IF(E_UNEXPECTED, link->empty());
            symlinks++;
            THROW_WIN32_IF(
                ERROR_REPARSE_POINT_ENCOUNTERED,
                symlinks > rootMaxSymlinks);
            auto [newparts, newSuffixSep] = SplitPathInRoot(
                *link,
                parts.begin(),
                parts.begin() + i,
                parts.begin() + (i + 1),
                parts.end());
            if (i == parts.size() - 1) {
                // suffixSep contains any trailing path separator characters
                // in the link target.
                // If we are replacing the remainder of the path, retain these.
                // If we're replacing some intermediate component of the path,
                // ignore them, since intermediate components must always be
                // directories.
                suffixSep = std::move(newSuffixSep);
            }
            if (newparts.size() < i ||
                !std::equal(
                    parts.begin(),
                    parts.begin() + i,
                    newparts.begin())) {
                // Some component in the path which we have already traversed
                // has changed. We need to restart parsing from the root.
                i = 0;
                if (dirfd != rootfd) {
                    CloseHandle(dirfd);
                }
                dirfd = rootfd;
            }
            parts = std::move(newparts);
            continue;
        }

        i++;
    }
}

} // namespace winrooted
