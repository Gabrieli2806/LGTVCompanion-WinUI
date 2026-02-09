#pragma once
#include "TopologyPage.g.h"

namespace winrt::LGTVCompanionUI::implementation
{
    struct TopologyPage : TopologyPageT<TopologyPage>
    {
        TopologyPage();
    };
}

namespace winrt::LGTVCompanionUI::factory_implementation
{
    struct TopologyPage : TopologyPageT<TopologyPage, implementation::TopologyPage>
    {
    };
}
