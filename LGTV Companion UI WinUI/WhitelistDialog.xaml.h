#pragma once
#include "WhitelistDialog.g.h"

namespace winrt::LGTVCompanionUI::implementation
{
    struct WhitelistDialog : WhitelistDialogT<WhitelistDialog>
    {
        WhitelistDialog();
    };
}

namespace winrt::LGTVCompanionUI::factory_implementation
{
    struct WhitelistDialog : WhitelistDialogT<WhitelistDialog, implementation::WhitelistDialog>
    {
    };
}
