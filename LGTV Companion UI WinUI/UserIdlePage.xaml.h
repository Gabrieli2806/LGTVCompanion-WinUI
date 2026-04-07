#pragma once
#include "UserIdlePage.g.h"

namespace winrt::LGTVCompanionUI::implementation
{
    struct UserIdlePage : UserIdlePageT<UserIdlePage>
    {
        UserIdlePage();
    };
}

namespace winrt::LGTVCompanionUI::factory_implementation
{
    struct UserIdlePage : UserIdlePageT<UserIdlePage, implementation::UserIdlePage>
    {
    };
}
