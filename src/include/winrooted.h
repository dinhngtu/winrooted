#pragma once

#include <Windows.h>

struct winrooted_Root;

EXTERN_C_START

#define WINROOTED_VERSION 0x00000000ul
// Note: Provisional (unstable)
#define WINROOTED_ABI_MASK_MAJOR 0x00000000ul

ULONG winrooted_version();

static inline bool winrooted_check_version() {
    ULONG version = winrooted_version();
    if ((version & WINROOTED_ABI_MASK_MAJOR) == 0) {
        return version == WINROOTED_VERSION;
    }
    return (version & WINROOTED_ABI_MASK_MAJOR) == (WINROOTED_VERSION & WINROOTED_ABI_MASK_MAJOR);
}

#define WR_O_RDONLY 0x00000
#define WR_O_WRONLY 0x00001
#define WR_O_RDWR 0x00002
#define WR_O_CREAT 0x00040
#define WR_O_CREATE WR_O_CREAT
#define WR_O_EXCL 0x00080
// #define WR_O_NOCTTY 0x00100
#define WR_O_TRUNC 0x00200
// #define WR_O_NONBLOCK 0x00800
#define WR_O_APPEND 0x00400
#define WR_O_SYNC 0x01000
// #define WR_O_ASYNC 0x02000
#define WR_O_CLOEXEC 0x80000
#define WR_O_DIRECTORY 0x04000 // target must be a directory

#define WR_O_FILE_FLAG_OPEN_NO_RECALL FILE_FLAG_OPEN_NO_RECALL
#define WR_O_FILE_FLAG_OPEN_REPARSE_POINT FILE_FLAG_OPEN_REPARSE_POINT
#define WR_O_FILE_FLAG_SESSION_AWARE FILE_FLAG_SESSION_AWARE
#define WR_O_FILE_FLAG_POSIX_SEMANTICS FILE_FLAG_POSIX_SEMANTICS
#define WR_O_FILE_FLAG_BACKUP_SEMANTICS FILE_FLAG_BACKUP_SEMANTICS
#define WR_O_FILE_FLAG_DELETE_ON_CLOSE FILE_FLAG_DELETE_ON_CLOSE
#define WR_O_FILE_FLAG_SEQUENTIAL_SCAN FILE_FLAG_SEQUENTIAL_SCAN
#define WR_O_FILE_FLAG_RANDOM_ACCESS FILE_FLAG_RANDOM_ACCESS
#define WR_O_FILE_FLAG_NO_BUFFERING FILE_FLAG_NO_BUFFERING
#define WR_O_FILE_FLAG_OVERLAPPED FILE_FLAG_OVERLAPPED
#define WR_O_FILE_FLAG_WRITE_THROUGH FILE_FLAG_WRITE_THROUGH

HRESULT winrooted_Root_New(_In_ PCWSTR name, _Outptr_ struct winrooted_Root **root);
void winrooted_Root_Delete(_In_opt_ _Post_ptr_invalid_ struct winrooted_Root *root);

HANDLE winrooted_Root_Handle(_In_ const struct winrooted_Root *root);
PCWSTR winrooted_Root_Name(_In_ const struct winrooted_Root *root);

// NOTICE: winrooted extension
// WINROOTED_IN_ROOT_FUNC must not throw a C++ exception. Doing so is fatal.
typedef HRESULT (*WINROOTED_IN_ROOT_FUNC)(
    // Handle of directory containing the last path element. Do not close this handle.
    _In_ HANDLE parent,
    // Name of the last path element.
    // If `name` is a symlink, function must return HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED).
    _In_ PCWSTR name,
    // Context passed from winrooted_Root_DoInRoot.
    _Inout_opt_ void *context,
    // Path to symlink to be followed.
    // Must be valid iff function returns HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED). Not doing so is fatal.
    // The link string will be freed with free().
    _At_(*link, _When_(return == __HRESULT_FROM_WIN32(ERROR_REPARSE_POINT_ENCOUNTERED), _Post_notnull_)) PWSTR *link);

// NOTICE: winrooted extension
HRESULT winrooted_Root_DoInRoot(
    _In_ const struct winrooted_Root *root,
    _In_ PCWSTR name,
    _In_ WINROOTED_IN_ROOT_FUNC func,
    _Inout_opt_ void *context);

// Note: You may only pass the `WR_O_*` flags to `flag`.
// Any `perm` bits other than `0777` are also not supported.
// Currently, only `S_IWRITE` really matters, and it only sets `FILE_ATTRIBUTE_READONLY` anyway.
HRESULT winrooted_Root_OpenFile(
    _In_ const struct winrooted_Root *root,
    _In_ PCWSTR name,
    _In_ int flag,
    _In_ ULONG perm,
    _Outptr_ HANDLE *file);

EXTERN_C_END

#ifdef __cplusplus
namespace winrooted {

class Root {
public:
    Root() {}
    Root(const Root &) = delete;
    Root &operator=(const Root &) = delete;
    Root(Root &&other) {
        swap(*this, other);
    }
    Root &operator=(Root &&other) {
        if (this->_root != other._root) {
            dispose();
            swap(*this, other);
        }
        return *this;
    }
    ~Root() {
        dispose();
    }

    static HRESULT New(_In_ PCWSTR name, _Out_ Root &result) {
        struct winrooted_Root *root;
        HRESULT hr = winrooted_Root_New(name, &root);
        if (SUCCEEDED(hr)) {
            result._root = root;
        }
        return hr;
    }

    HANDLE Handle() const {
        return winrooted_Root_Handle(_root);
    }

    PCWSTR Name() const {
        return winrooted_Root_Name(_root);
    }

    HRESULT OpenFile(_In_ PCWSTR name, _In_ int flag, _In_ ULONG perm, _Outptr_ HANDLE *file) const {
        return winrooted_Root_OpenFile(_root, name, flag, perm, file);
    }

    HRESULT Open(_In_ PCWSTR name, _Outptr_ HANDLE *file) const {
        return OpenFile(name, WR_O_RDONLY, 0, file);
    }

    HRESULT Create(_In_ PCWSTR name, _Outptr_ HANDLE *file) const {
        return OpenFile(name, WR_O_RDWR | WR_O_CREAT | WR_O_TRUNC, 0666, file);
    }

    HRESULT DoInRoot(_In_ PCWSTR name, _In_ WINROOTED_IN_ROOT_FUNC func, _Inout_opt_ void *context) const {
        return winrooted_Root_DoInRoot(_root, name, func, context);
    }

    struct winrooted_Root *GetAbi() {
        return _root;
    }

    const struct winrooted_Root *GetAbi() const {
        return _root;
    }

    friend void swap(Root &self, Root &other) {
        struct winrooted_Root *tmp;
        tmp = self._root;
        self._root = other._root;
        other._root = tmp;
    }

private:
    struct winrooted_Root *_root = nullptr;

    void dispose() {
        if (_root) {
            winrooted_Root_Delete(_root);
        }
        _root = nullptr;
    }
};

} // namespace winrooted
#endif
