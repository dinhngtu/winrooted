#include "stdafx.h"

#include "winrooted.h"
#include "root.hpp"

#include <wil/result_macros.h>
#include <wil/resource.h>

struct winrooted_Root {
    winrooted::detail::Root root;
};

EXTERN_C_START

ULONG winrooted_version() {
    return WINROOTED_VERSION;
}

HRESULT winrooted_Root_New(_In_ PCWSTR name, _Outptr_ struct winrooted_Root **root) {
    *root = new (std::nothrow) winrooted_Root();
    RETURN_IF_NULL_ALLOC(*root);
    return winrooted::detail::Root::New(std::wstring_view(name), (*root)->root);
}

void winrooted_Root_Delete(_In_opt_ _Post_ptr_invalid_ struct winrooted_Root *root) {
    delete root;
}

HANDLE winrooted_Root_Handle(_In_ const struct winrooted_Root *root) {
    return root->root.Handle();
}

PCWSTR winrooted_Root_Name(_In_ const struct winrooted_Root *root) {
    return root->root.Name().c_str();
}

HRESULT winrooted_Root_DoInRoot(
    _In_ const struct winrooted_Root *root,
    _In_ PCWSTR name,
    _In_ WINROOTED_IN_ROOT_FUNC func,
    _Inout_opt_ void *context) {
    RETURN_HR(root->root.DoInRootAbi(name, func, context));
}

HRESULT winrooted_Root_OpenFile(
    _In_ const struct winrooted_Root *root,
    _In_ PCWSTR name,
    _In_ int flag,
    _In_ ULONG perm,
    _Outptr_ HANDLE *file) {
    std::wstring_view vname(name);
    wil::unique_hfile h;
    RETURN_IF_FAILED(root->root.OpenFile(vname, flag, perm, h));
    *file = h.release();
    return S_OK;
}

EXTERN_C_END
