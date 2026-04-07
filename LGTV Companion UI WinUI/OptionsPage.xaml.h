#pragma once
#include "OptionsPage.g.h"

namespace winrt::LGTVCompanionUI::implementation
{
    struct OptionsPage : OptionsPageT<OptionsPage>
    {
        OptionsPage();
    };
}

namespace winrt::LGTVCompanionUI::factory_implementation
{
    struct OptionsPage : OptionsPageT<OptionsPage, implementation::OptionsPage>
    {
    };
}
