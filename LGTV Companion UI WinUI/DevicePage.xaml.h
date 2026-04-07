#pragma once
#include "DevicePage.g.h"

namespace winrt::LGTVCompanionUI::implementation
{
    struct DevicePage : DevicePageT<DevicePage>
    {
        DevicePage();
    };
}

namespace winrt::LGTVCompanionUI::factory_implementation
{
    struct DevicePage : DevicePageT<DevicePage, implementation::DevicePage>
    {
    };
}
