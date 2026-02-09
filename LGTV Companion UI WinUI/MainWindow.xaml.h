#pragma once
#include "MainWindow.g.h"
#include "../Common/preferences.h"
#include "../Common/ipc_v2.h"
#include "../Common/common_app_define.h"
#include "../Common/tools.h"
#include "../Common/log.h"

namespace winrt::LGTVCompanionUI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // Event handlers
        void DeviceComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void EnableCheckBox_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void ConfigureSplitButton_Click(winrt::Microsoft::UI::Xaml::Controls::SplitButton const& sender, winrt::Microsoft::UI::Xaml::Controls::SplitButtonClickEventArgs const& args);
        void SettingsButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void ApplyButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void DonateLink_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void NewVersionLink_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Menu flyout handlers
        void MenuManage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void MenuAdd_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void MenuScan_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void MenuRemove_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void MenuRemoveAll_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void MenuTest_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void MenuTurnOn_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void MenuTurnOff_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        void RefreshDeviceList();
        void CommunicateWithService(std::wstring sData);
        void ThreadVersionCheck();
        void ScanForDevices(bool replaceExisting);
        winrt::Windows::Foundation::IAsyncAction ShowMessageDialogAsync(winrt::hstring message, winrt::hstring title);
        winrt::Windows::Foundation::IAsyncOperation<winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult> ShowConfirmDialogAsync(winrt::hstring message, winrt::hstring title, winrt::hstring primaryText, winrt::hstring secondaryText);
        winrt::Windows::Foundation::IAsyncAction ApplyChangesAsync();
        winrt::Windows::Foundation::IAsyncAction ShowDeviceDialogAsync(bool isAdd);
        winrt::Windows::Foundation::IAsyncAction ShowOptionsDialogAsync();
        void TurnOnDevice();
        void TurnOffDevice();
    };
}

namespace winrt::LGTVCompanionUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
