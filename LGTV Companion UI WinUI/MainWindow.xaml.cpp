#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <Shlobj_core.h>
#include <shellapi.h>
#include <SetupAPI.h>
#include <initguid.h>
#include <functiondiscoverykeys.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <urlmon.h>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Shell32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Foundation;

// Global state shared across the WinUI3 application
static Preferences Prefs(CONFIG_FILE);
static std::shared_ptr<IpcClient2> p_pipe_client;
static std::shared_ptr<Logging> logger;
static bool reset_api_keys = false;

namespace winrt::LGTVCompanionUI::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Set window title
        std::wstring title = APPNAME;
        title += L" v";
        title += APP_VERSION;
        this->Title(title);

        // Initialize logger
        std::wstring file = tools::widen(Prefs.data_path_);
        file += L"ui_log.txt";
        logger = std::make_shared<Logging>(0, file);

        // Initialize IPC
        auto callback = [](std::wstring, LPVOID) {};
        p_pipe_client = std::make_shared<IpcClient2>(PIPENAME, callback, (LPVOID)nullptr);

        // Initialize Winsock
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        // Topology tweak
        bool bTop = false;
        for (auto& m : Prefs.devices_)
        {
            if (m.uniqueDeviceKey != "")
                bTop = true;
        }
        Prefs.topology_support_ = bTop ? Prefs.topology_support_ : false;

        // Populate device list
        RefreshDeviceList();

        // Check for updates
        if (Prefs.updater_mode_ != PREFS_UPDATER_OFF)
        {
            std::thread thread_obj([this]() { ThreadVersionCheck(); });
            thread_obj.detach();
        }
    }

    void MainWindow::RefreshDeviceList()
    {
        DeviceComboBox().Items().Clear();

        if (Prefs.devices_.size() > 0)
        {
            for (const auto& item : Prefs.devices_)
            {
                DeviceComboBox().Items().Append(winrt::box_value(winrt::to_hstring(item.name)));
            }
            DeviceComboBox().SelectedIndex(0);
            DeviceComboBox().IsEnabled(true);
            EnableCheckBox().IsEnabled(true);
            EnableCheckBox().IsChecked(Prefs.devices_[0].enabled);
            ConfigureSplitButton().Content(winrt::box_value(L"Configure"));
        }
        else
        {
            DeviceComboBox().IsEnabled(false);
            EnableCheckBox().IsEnabled(false);
            ConfigureSplitButton().Content(winrt::box_value(L"Scan"));
        }
    }

    void MainWindow::DeviceComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        int sel = DeviceComboBox().SelectedIndex();
        if (sel >= 0 && sel < static_cast<int>(Prefs.devices_.size()))
        {
            EnableCheckBox().IsEnabled(true);
            EnableCheckBox().IsChecked(Prefs.devices_[sel].enabled);
        }
    }

    void MainWindow::EnableCheckBox_Click(IInspectable const&, RoutedEventArgs const&)
    {
        int sel = DeviceComboBox().SelectedIndex();
        if (sel >= 0 && sel < static_cast<int>(Prefs.devices_.size()))
        {
            Prefs.devices_[sel].enabled = EnableCheckBox().IsChecked().Value();
            ApplyButton().IsEnabled(true);
        }
    }

    void MainWindow::ConfigureSplitButton_Click(SplitButton const&, SplitButtonClickEventArgs const&)
    {
        if (Prefs.devices_.size() > 0)
        {
            ShowDeviceDialogAsync(false);
        }
        else
        {
            MenuScan_Click(nullptr, nullptr);
        }
    }

    void MainWindow::SettingsButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ShowOptionsDialogAsync();
    }

    void MainWindow::ApplyButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ApplyChangesAsync();
    }

    void MainWindow::DonateLink_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ShowConfirmDialogAsync(
            L"This is free software, but your support is appreciated and there "
            "is a donation page set up over at PayPal. PayPal allows you to use a credit- or debit "
            "card or your PayPal balance to make a donation, even without a PayPal account.\n\n"
            "Click 'Yes' to continue to the PayPal donation web page (a PayPal account is not "
            "required to make a donation)!"
            "\n\nAlternatively you can go to the following URL in your web browser (a PayPal "
            "account is required for this link).\n\nhttps://paypal.me/jpersson77",
            L"Donate via PayPal?",
            L"Yes",
            L"No"
        ).Completed([](auto const& asyncOp, auto) {
            if (asyncOp.GetResults() == ContentDialogResult::Primary)
            {
                ShellExecute(0, 0, DONATELINK, 0, 0, SW_SHOW);
            }
        });
    }

    void MainWindow::NewVersionLink_Click(IInspectable const&, RoutedEventArgs const&)
    {
        TCHAR buffer[MAX_PATH] = { 0 };
        if (GetModuleFileName(NULL, buffer, MAX_PATH))
        {
            std::wstring path = buffer;
            std::wstring::size_type pos = path.find_last_of(L"\\/");
            path = path.substr(0, pos + 1);
            std::wstring exe = path;
            exe += L"LGTVupdater.exe";
            ShellExecute(NULL, L"open", exe.c_str(), NULL, path.c_str(), SW_SHOW);
        }
    }

    void MainWindow::MenuManage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (Prefs.devices_.size() > 0 && DeviceComboBox().SelectedIndex() >= 0)
        {
            ShowDeviceDialogAsync(false);
        }
    }

    void MainWindow::MenuAdd_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ShowDeviceDialogAsync(true);
    }

    void MainWindow::MenuScan_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (Prefs.devices_.size() > 0)
        {
            ShowConfirmDialogAsync(
                L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nDo you want to replace the current devices with any discovered devices?\n\nYES = clear current devices before adding, \n\nNO = add to current list of devices.",
                L"Scanning",
                L"Yes",
                L"No"
            ).Completed([this](auto const& asyncOp, auto) {
                auto result = asyncOp.GetResults();
                if (result == ContentDialogResult::Primary)
                {
                    this->DispatcherQueue().TryEnqueue([this]() { ScanForDevices(true); });
                }
                else if (result == ContentDialogResult::Secondary)
                {
                    this->DispatcherQueue().TryEnqueue([this]() { ScanForDevices(false); });
                }
            });
        }
        else
        {
            ShowConfirmDialogAsync(
                L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nClick OK to continue!",
                L"Scanning",
                L"OK",
                L"Cancel"
            ).Completed([this](auto const& asyncOp, auto) {
                if (asyncOp.GetResults() == ContentDialogResult::Primary)
                {
                    this->DispatcherQueue().TryEnqueue([this]() { ScanForDevices(true); });
                }
            });
        }
    }

    void MainWindow::MenuRemove_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (Prefs.devices_.size() > 0)
        {
            ShowConfirmDialogAsync(
                L"You are about to remove this device.\n\nDo you want to continue?",
                L"Remove device",
                L"Yes",
                L"No"
            ).Completed([this](auto const& asyncOp, auto) {
                if (asyncOp.GetResults() == ContentDialogResult::Primary)
                {
                    this->DispatcherQueue().TryEnqueue([this]() {
                        int sel = DeviceComboBox().SelectedIndex();
                        if (sel >= 0 && sel < static_cast<int>(Prefs.devices_.size()))
                        {
                            Prefs.devices_.erase(Prefs.devices_.begin() + sel);
                            RefreshDeviceList();
                            ApplyButton().IsEnabled(true);
                        }
                    });
                }
            });
        }
    }

    void MainWindow::MenuRemoveAll_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (Prefs.devices_.size() > 0)
        {
            ShowConfirmDialogAsync(
                L"You are about to remove ALL devices.\n\nDo you want to continue?",
                L"Remove all devices",
                L"Yes",
                L"No"
            ).Completed([this](auto const& asyncOp, auto) {
                if (asyncOp.GetResults() == ContentDialogResult::Primary)
                {
                    this->DispatcherQueue().TryEnqueue([this]() {
                        Prefs.devices_.clear();
                        RefreshDeviceList();
                        ApplyButton().IsEnabled(true);
                    });
                }
            });
        }
    }

    void MainWindow::MenuTest_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (Prefs.devices_.size() > 0 && !ApplyButton().IsEnabled())
        {
            ShowConfirmDialogAsync(
                L"You are about to test the ability to control this device?\n\nPlease click YES to power off the device. Then wait about 5 seconds and press Enter to power on the device again.",
                L"Test device",
                L"Yes",
                L"No"
            ).Completed([this](auto const& asyncOp, auto) {
                if (asyncOp.GetResults() == ContentDialogResult::Primary)
                {
                    this->DispatcherQueue().TryEnqueue([this]() {
                        TurnOffDevice();
                        ShowConfirmDialogAsync(
                            L"Please press OK to power on the device again.",
                            L"Test device",
                            L"OK",
                            L""
                        ).Completed([this](auto const&, auto) {
                            this->DispatcherQueue().TryEnqueue([this]() { TurnOnDevice(); });
                        });
                    });
                }
            });
        }
        else if (ApplyButton().IsEnabled())
        {
            ShowMessageDialogAsync(L"Please apply unsaved changes before attempting to control the device", L"Information");
        }
    }

    void MainWindow::MenuTurnOn_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (Prefs.devices_.size() > 0)
        {
            if (ApplyButton().IsEnabled())
            {
                ShowMessageDialogAsync(L"Please apply unsaved changes before attempting to control the device", L"Information");
                return;
            }
            TurnOnDevice();
        }
    }

    void MainWindow::MenuTurnOff_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (Prefs.devices_.size() > 0)
        {
            if (ApplyButton().IsEnabled())
            {
                ShowMessageDialogAsync(L"Please apply unsaved changes before attempting to control the device", L"Information");
                return;
            }
            TurnOffDevice();
        }
    }

    void MainWindow::TurnOnDevice()
    {
        int sel = DeviceComboBox().SelectedIndex();
        if (sel >= 0 && sel < static_cast<int>(Prefs.devices_.size()))
        {
            std::wstring s = L"-poweron ";
            s += tools::widen(Prefs.devices_[sel].id);
            CommunicateWithService(s);
        }
    }

    void MainWindow::TurnOffDevice()
    {
        int sel = DeviceComboBox().SelectedIndex();
        if (sel >= 0 && sel < static_cast<int>(Prefs.devices_.size()))
        {
            std::wstring s = L"-poweroff ";
            s += tools::widen(Prefs.devices_[sel].id);
            CommunicateWithService(s);
        }
    }

    void MainWindow::CommunicateWithService(std::wstring sData)
    {
        int max_wait = 1000;
        while (!p_pipe_client->send(sData) && max_wait > 0)
        {
            Sleep(25);
            max_wait -= 25;
        }
        if (max_wait <= 0)
        {
            ShowMessageDialogAsync(L"Failed to connect to named pipe. Service may be stopped.", L"Error");
        }
    }

    void MainWindow::ThreadVersionCheck()
    {
        IStream* stream;
        char buff[100];
        std::string s;
        unsigned long bytesRead;
        nlohmann::json json_data;

        if (URLOpenBlockingStream(0, VERSIONCHECKLINK, &stream, 0, 0))
            return;

        while (true)
        {
            stream->Read(buff, 100, &bytesRead);
            if (0U == bytesRead)
                break;
            s.append(buff, bytesRead);
        };
        stream->Release();

        try
        {
            json_data = nlohmann::json::parse(s);
        }
        catch (...)
        {
            return;
        }
        if (!json_data["tag_name"].empty() && json_data["tag_name"].is_string())
        {
            std::string remote = json_data["tag_name"];
            std::vector<std::string> local_ver = tools::stringsplit(tools::narrow(APP_VERSION), ".");
            std::vector<std::string> remote_ver = tools::stringsplit(remote, "v.");

            if (local_ver.size() < 3 || remote_ver.size() < 3)
                return;

            int local_ver_major = atoi(local_ver[0].c_str());
            int local_ver_minor = atoi(local_ver[1].c_str());
            int local_ver_patch = atoi(local_ver[2].c_str());

            int remote_ver_major = atoi(remote_ver[0].c_str());
            int remote_ver_minor = atoi(remote_ver[1].c_str());
            int remote_ver_patch = atoi(remote_ver[2].c_str());

            if ((remote_ver_major > local_ver_major) ||
                (remote_ver_major == local_ver_major && remote_ver_minor > local_ver_minor) ||
                (remote_ver_major == local_ver_major && remote_ver_minor == local_ver_minor && remote_ver_patch > local_ver_patch))
            {
                this->DispatcherQueue().TryEnqueue([this]() {
                    NewVersionLink().Visibility(Visibility::Visible);
                });
            }
        }
    }

    void MainWindow::ScanForDevices(bool replaceExisting)
    {
        int devicesAdded = 0;
        bool changesWereMade = false;

        HDEVINFO DeviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES);
        SP_DEVINFO_DATA DeviceInfoData;
        memset(&DeviceInfoData, 0, sizeof(SP_DEVINFO_DATA));
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        int DeviceIndex = 0;

        while (SetupDiEnumDeviceInfo(DeviceInfoSet, DeviceIndex, &DeviceInfoData))
        {
            PDEVPROPKEY pDevPropKey;
            DEVPROPTYPE PropType;
            DWORD required_size = 0;
            DeviceIndex++;

            pDevPropKey = (PDEVPROPKEY)(&DEVPKEY_Device_FriendlyName);
            SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
            if (required_size > 2)
            {
                std::vector<BYTE> unicode_buffer(required_size, 0);
                SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer.data(), required_size, nullptr, 0);
                std::wstring FriendlyName = ((PWCHAR)unicode_buffer.data());
                if (FriendlyName.find(L"[LG]", 0) != std::wstring::npos)
                {
                    pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_IpAddress);
                    SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
                    if (required_size > 2)
                    {
                        std::vector<BYTE> unicode_buffer2(required_size, 0);
                        SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer2.data(), required_size, nullptr, 0);
                        std::wstring IP = ((PWCHAR)unicode_buffer2.data());
                        pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_PhysicalAddress);
                        SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
                        if (required_size >= 6)
                        {
                            std::vector<BYTE> unicode_buffer3(required_size, 0);
                            SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer3.data(), required_size, nullptr, 0);

                            std::stringstream ss;
                            std::string MAC;
                            ss << std::hex << std::setfill('0');
                            for (int i = 0; i < 6; i++)
                            {
                                ss << std::setw(2) << static_cast<unsigned>(unicode_buffer3[i]);
                                if (i < 5) ss << ":";
                            }
                            MAC = ss.str();
                            transform(MAC.begin(), MAC.end(), MAC.begin(), ::toupper);
                            if (replaceExisting)
                            {
                                Prefs.devices_.clear();
                                replaceExisting = false;
                                changesWereMade = true;
                            }
                            bool DeviceExists = false;
                            for (auto& item : Prefs.devices_)
                                for (auto& m : item.mac_addresses)
                                    if (m == MAC)
                                    {
                                        if (tools::narrow(IP) != item.ip)
                                        {
                                            item.ip = tools::narrow(IP);
                                            changesWereMade = true;
                                        }
                                        DeviceExists = true;
                                    }
                            if (!DeviceExists)
                            {
                                Device temp;
                                temp.name = tools::narrow(FriendlyName);
                                std::string prefix = "[LG] webOS TV ";
                                if (temp.name.find(prefix) == 0)
                                {
                                    if (temp.name.size() > 18)
                                        temp.name.erase(0, prefix.length());
                                }
                                temp.ip = tools::narrow(IP);
                                temp.mac_addresses.push_back(MAC);
                                temp.subnet = tools::getSubnetMask(temp.ip);
                                temp.wake_method = WOL_TYPE_AUTO;
                                Prefs.devices_.push_back(temp);
                                changesWereMade = true;
                                devicesAdded++;
                            }
                        }
                    }
                }
            }
        }

        if (changesWereMade)
        {
            RefreshDeviceList();
            ApplyButton().IsEnabled(true);
        }
        if (DeviceInfoSet)
            SetupDiDestroyDeviceInfoList(DeviceInfoSet);

        std::wstringstream mess;
        mess << devicesAdded << L" new devices found.";
        ShowMessageDialogAsync(winrt::hstring(mess.str()), L"Scan results");
    }

    IAsyncAction MainWindow::ApplyChangesAsync()
    {
        // Check subnet warnings
        std::vector<std::string> IPs = tools::getLocalIP();
        if (IPs.size() > 0)
        {
            for (auto& Dev : Prefs.devices_)
            {
                bool bFound = false;
                for (auto& IP : IPs)
                {
                    std::vector temp = tools::stringsplit(IP, "/");
                    if (temp.size() > 1)
                    {
                        std::string ip_addr = temp[0];
                        std::string cidr = temp[1];
                        std::stringstream subnet;
                        unsigned long mask = (0xFFFFFFFF << (32 - atoi(cidr.c_str())) & 0xFFFFFFFF);
                        subnet << (mask >> 24) << "." << ((mask >> 16) & 0xFF) << "." << ((mask >> 8) & 0xFF) << "." << (mask & 0xFF);
                        if (tools::isSameSubnet(Dev.ip.c_str(), ip_addr.c_str(), subnet.str().c_str()))
                            bFound = true;
                    }
                    else
                        bFound = true;
                }
                if (!bFound)
                {
                    std::string mess = Dev.id;
                    mess += " with name \"";
                    mess += Dev.name;
                    mess += "\" and IP ";
                    mess += Dev.ip;
                    mess += " is not on the same subnet as the PC. Please note that this might cause problems with waking "
                        "up the TV. Please check the documentation and the configuration.\n\n Do you want to continue anyway?";

                    auto result = co_await ShowConfirmDialogAsync(winrt::to_hstring(mess), L"Warning", L"Yes", L"No");
                    if (result != ContentDialogResult::Primary)
                        co_return;
                }
            }
        }

        Prefs.writeToDisk();

        // Restart the service
        SERVICE_STATUS_PROCESS status;
        DWORD bytesNeeded;
        SC_HANDLE serviceDbHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (serviceDbHandle)
        {
            SC_HANDLE serviceHandle = OpenService(serviceDbHandle, L"LGTVsvc", SERVICE_QUERY_STATUS | SERVICE_STOP | SERVICE_START);
            if (serviceHandle)
            {
                QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded);
                ControlService(serviceHandle, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&status);
                while (status.dwCurrentState != SERVICE_STOPPED)
                {
                    Sleep(100);
                    if (QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded))
                    {
                        if (status.dwCurrentState == SERVICE_STOPPED)
                            break;
                    }
                }
                StartService(serviceHandle, NULL, NULL);
                CloseServiceHandle(serviceHandle);
            }
            else
            {
                co_await ShowMessageDialogAsync(L"The LGTV Companion service is not installed. Please reinstall the application", L"Error");
            }
            CloseServiceHandle(serviceDbHandle);
        }

        // Restart the daemon
        UINT custom_daemon_restart_message = RegisterWindowMessage(CUSTOM_MESSAGE_RESTART);
        std::wstring window_title = APPNAME;
        window_title += L" Daemon v";
        window_title += APP_VERSION;
        HWND daemon_hWnd = FindWindow(NULL, window_title.c_str());
        if (daemon_hWnd)
        {
            PostMessage(daemon_hWnd, custom_daemon_restart_message, NULL, NULL);
        }
        else
        {
            TCHAR buffer[MAX_PATH] = { 0 };
            if (GetModuleFileName(NULL, buffer, MAX_PATH))
            {
                std::wstring path = buffer;
                std::wstring::size_type pos = path.find_last_of(L"\\/");
                path = path.substr(0, pos + 1);
                std::wstring exe = path;
                exe += L"LGTVdaemon.exe";
                ShellExecute(NULL, L"open", exe.c_str(), L"-restart", path.c_str(), SW_HIDE);
            }
        }

        ApplyButton().IsEnabled(false);
    }

    IAsyncAction MainWindow::ShowMessageDialogAsync(hstring message, hstring title)
    {
        ContentDialog dialog;
        dialog.XamlRoot(this->Content().XamlRoot());
        dialog.Title(winrt::box_value(title));
        dialog.Content(winrt::box_value(message));
        dialog.CloseButtonText(L"OK");
        co_await dialog.ShowAsync();
    }

    IAsyncOperation<ContentDialogResult> MainWindow::ShowConfirmDialogAsync(hstring message, hstring title, hstring primaryText, hstring secondaryText)
    {
        ContentDialog dialog;
        dialog.XamlRoot(this->Content().XamlRoot());
        dialog.Title(winrt::box_value(title));
        dialog.Content(winrt::box_value(message));
        dialog.PrimaryButtonText(primaryText);
        if (secondaryText.size() > 0)
            dialog.SecondaryButtonText(secondaryText);
        dialog.DefaultButton(ContentDialogButton::Primary);
        auto result = co_await dialog.ShowAsync();
        co_return result;
    }

    IAsyncAction MainWindow::ShowDeviceDialogAsync(bool isAdd)
    {
        int sel = DeviceComboBox().SelectedIndex();
        if (!isAdd && (sel < 0 || sel >= static_cast<int>(Prefs.devices_.size())))
            co_return;

        // Build the device configuration dialog content
        StackPanel panel;
        panel.Spacing(8);

        // Device name
        TextBlock nameLabel;
        nameLabel.Text(L"Device name:");
        panel.Children().Append(nameLabel);
        TextBox nameBox;
        nameBox.PlaceholderText(L"Device name");
        if (!isAdd) nameBox.Text(winrt::to_hstring(Prefs.devices_[sel].name));
        panel.Children().Append(nameBox);

        // IP address
        TextBlock ipLabel;
        ipLabel.Text(L"IP address:");
        panel.Children().Append(ipLabel);
        TextBox ipBox;
        ipBox.PlaceholderText(L"192.168.1.100");
        if (!isAdd) ipBox.Text(winrt::to_hstring(Prefs.devices_[sel].ip));
        panel.Children().Append(ipBox);

        // MAC addresses
        TextBlock macLabel;
        macLabel.Text(L"MAC address(es):");
        panel.Children().Append(macLabel);
        TextBox macBox;
        macBox.AcceptsReturn(true);
        macBox.TextWrapping(TextWrapping::Wrap);
        macBox.Height(80);
        if (!isAdd)
        {
            std::wstring macStr;
            for (const auto& mac : Prefs.devices_[sel].mac_addresses)
            {
                macStr += tools::widen(mac);
                if (&mac != &Prefs.devices_[sel].mac_addresses.back())
                    macStr += L"\r\n";
            }
            macBox.Text(winrt::hstring(macStr));
        }
        panel.Children().Append(macBox);

        // WOL method
        TextBlock wolLabel;
        wolLabel.Text(L"Wake on LAN method:");
        panel.Children().Append(wolLabel);
        ComboBox wolCombo;
        wolCombo.Items().Append(winrt::box_value(L"Automatic"));
        wolCombo.Items().Append(winrt::box_value(L"Manual"));
        wolCombo.SelectedIndex(!isAdd && Prefs.devices_[sel].wake_method != WOL_TYPE_AUTO ? 1 : 0);
        panel.Children().Append(wolCombo);

        // SSL mode
        TextBlock sslLabel;
        sslLabel.Text(L"Firmware:");
        panel.Children().Append(sslLabel);
        ComboBox sslCombo;
        sslCombo.Items().Append(winrt::box_value(L"Default"));
        sslCombo.Items().Append(winrt::box_value(L"Legacy"));
        sslCombo.SelectedIndex(!isAdd && !Prefs.devices_[sel].ssl ? 1 : 0);
        panel.Children().Append(sslCombo);

        // Persistence
        TextBlock persLabel;
        persLabel.Text(L"Persistence:");
        panel.Children().Append(persLabel);
        ComboBox persCombo;
        persCombo.Items().Append(winrt::box_value(L"Off"));
        persCombo.Items().Append(winrt::box_value(L"Persistent"));
        persCombo.Items().Append(winrt::box_value(L"Persistent + Keep Alive"));
        persCombo.SelectedIndex(!isAdd ? Prefs.devices_[sel].persistent_connection_level : 0);
        panel.Children().Append(persCombo);

        // Source HDMI input
        TextBlock sourceLabel;
        sourceLabel.Text(L"Source HDMI input:");
        panel.Children().Append(sourceLabel);
        ComboBox sourceCombo;
        sourceCombo.Items().Append(winrt::box_value(L"HDMI 1"));
        sourceCombo.Items().Append(winrt::box_value(L"HDMI 2"));
        sourceCombo.Items().Append(winrt::box_value(L"HDMI 3"));
        sourceCombo.Items().Append(winrt::box_value(L"HDMI 4"));
        sourceCombo.SelectedIndex(!isAdd ? Prefs.devices_[sel].sourceHdmiInput - 1 : 0);
        panel.Children().Append(sourceCombo);

        // HDMI input check
        CheckBox hdmiCheck;
        hdmiCheck.Content(winrt::box_value(L"Do not automatically power off device when using it for other activities"));
        hdmiCheck.IsChecked(!isAdd && Prefs.devices_[sel].check_hdmi_input_when_power_off);
        panel.Children().Append(hdmiCheck);

        // Set HDMI input on power on
        CheckBox setHdmiCheck;
        setHdmiCheck.Content(winrt::box_value(L"Switch to PC HDMI-input when powering on"));
        setHdmiCheck.IsChecked(!isAdd && Prefs.devices_[sel].set_hdmi_input_on_power_on);
        panel.Children().Append(setHdmiCheck);

        // HDMI delay
        TextBlock delayLabel;
        delayLabel.Text(L"HDMI switch delay (seconds):");
        panel.Children().Append(delayLabel);
        NumberBox delayBox;
        delayBox.Minimum(0);
        delayBox.Maximum(30);
        delayBox.Value(!isAdd ? Prefs.devices_[sel].set_hdmi_input_on_power_on_delay : 1);
        delayBox.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Inline);
        panel.Children().Append(delayBox);

        // Build the dialog
        ContentDialog dialog;
        dialog.XamlRoot(this->Content().XamlRoot());
        dialog.Title(winrt::box_value(isAdd ? L"Add device" : L"Configure device"));
        dialog.Content(panel);
        dialog.PrimaryButtonText(isAdd ? L"Add" : L"Save");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        ScrollViewer scrollViewer;
        scrollViewer.Content(panel);
        scrollViewer.MaxHeight(500);
        dialog.Content(scrollViewer);

        auto result = co_await dialog.ShowAsync();
        if (result == ContentDialogResult::Primary)
        {
            // Validate MAC addresses
            std::wstring edittext = std::wstring(macBox.Text());
            std::vector<std::string> maclines;

            if (!edittext.empty())
            {
                transform(edittext.begin(), edittext.end(), edittext.begin(), ::toupper);
                char CharsToRemove[] = ":- ,;.\n";
                for (int i = 0; i < strlen(CharsToRemove); ++i)
                    edittext.erase(remove(edittext.begin(), edittext.end(), CharsToRemove[i]), edittext.end());

                if (edittext.find_first_not_of(L"0123456789ABCDEF\r") != std::string::npos)
                {
                    co_await ShowMessageDialogAsync(L"One or several MAC addresses contain illegal characters.\n\nPlease correct before continuing.", L"Error");
                    co_return;
                }
                maclines = tools::stringsplit(tools::narrow(edittext), "\r");
                for (auto& mac : maclines)
                {
                    if (mac.length() != 12)
                    {
                        co_await ShowMessageDialogAsync(L"One or several MAC addresses is incorrect.\n\nPlease correct before continuing.", L"Error");
                        co_return;
                    }
                    else
                        for (int ind = 4; ind >= 0; ind--)
                            mac.insert(ind * 2 + 2, ":");
                }
            }

            std::string nameStr = winrt::to_string(nameBox.Text());
            std::string ipStr = winrt::to_string(ipBox.Text());

            if (ipStr == "0.0.0.0" || ipStr.empty())
            {
                co_await ShowMessageDialogAsync(L"The IP-address is invalid.\n\nPlease correct before continuing.", L"Error");
                co_return;
            }

            if (maclines.empty() || nameStr.empty() || ipStr.empty())
            {
                co_await ShowMessageDialogAsync(L"The configuration is incorrect or missing information.\n\nPlease correct before continuing.", L"Error");
                co_return;
            }

            if (isAdd)
            {
                Device sess;
                std::stringstream strs;
                strs << "Device" << Prefs.devices_.size() + 1;
                sess.id = strs.str();
                sess.mac_addresses = maclines;
                sess.name = nameStr;
                sess.ip = ipStr;
                sess.wake_method = wolCombo.SelectedIndex() == 0 ? WOL_TYPE_AUTO : WOL_TYPE_IP;
                sess.subnet = wolCombo.SelectedIndex() == 0 ? tools::getSubnetMask(sess.ip) : WOL_DEFAULT_SUBNET;
                sess.ssl = sslCombo.SelectedIndex() == 0;
                sess.persistent_connection_level = persCombo.SelectedIndex();
                sess.sourceHdmiInput = sourceCombo.SelectedIndex() + 1;
                sess.check_hdmi_input_when_power_off = hdmiCheck.IsChecked().Value();
                sess.set_hdmi_input_on_power_on = setHdmiCheck.IsChecked().Value();
                sess.set_hdmi_input_on_power_on_delay = static_cast<int>(delayBox.Value());
                Prefs.devices_.push_back(sess);
            }
            else
            {
                Prefs.devices_[sel].mac_addresses = maclines;
                Prefs.devices_[sel].name = nameStr;
                Prefs.devices_[sel].ip = ipStr;
                Prefs.devices_[sel].wake_method = wolCombo.SelectedIndex() == 0 ? WOL_TYPE_AUTO : WOL_TYPE_IP;
                Prefs.devices_[sel].subnet = wolCombo.SelectedIndex() == 0 ? tools::getSubnetMask(Prefs.devices_[sel].ip) : WOL_DEFAULT_SUBNET;
                Prefs.devices_[sel].ssl = sslCombo.SelectedIndex() == 0;
                Prefs.devices_[sel].persistent_connection_level = persCombo.SelectedIndex();
                Prefs.devices_[sel].sourceHdmiInput = sourceCombo.SelectedIndex() + 1;
                Prefs.devices_[sel].check_hdmi_input_when_power_off = hdmiCheck.IsChecked().Value();
                Prefs.devices_[sel].set_hdmi_input_on_power_on = setHdmiCheck.IsChecked().Value();
                Prefs.devices_[sel].set_hdmi_input_on_power_on_delay = static_cast<int>(delayBox.Value());
            }

            RefreshDeviceList();
            ApplyButton().IsEnabled(true);
        }
    }

    IAsyncAction MainWindow::ShowOptionsDialogAsync()
    {
        StackPanel panel;
        panel.Spacing(8);

        // Power on timeout
        TextBlock timeoutLabel;
        timeoutLabel.Text(L"Power on timeout (seconds):");
        panel.Children().Append(timeoutLabel);
        NumberBox timeoutBox;
        timeoutBox.Minimum(5);
        timeoutBox.Maximum(100);
        timeoutBox.Value(Prefs.power_on_timeout_);
        timeoutBox.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Inline);
        panel.Children().Append(timeoutBox);

        // Log level
        TextBlock logLabel;
        logLabel.Text(L"Log level:");
        panel.Children().Append(logLabel);
        ComboBox logCombo;
        logCombo.Items().Append(winrt::box_value(L"Off"));
        logCombo.Items().Append(winrt::box_value(L"Info"));
        logCombo.Items().Append(winrt::box_value(L"Warning"));
        logCombo.Items().Append(winrt::box_value(L"Error"));
        logCombo.Items().Append(winrt::box_value(L"Debug"));
        logCombo.SelectedIndex(Prefs.log_level_);
        panel.Children().Append(logCombo);

        // Updater mode
        TextBlock updateLabel;
        updateLabel.Text(L"Auto update:");
        panel.Children().Append(updateLabel);
        ComboBox updateCombo;
        updateCombo.Items().Append(winrt::box_value(L"Off"));
        updateCombo.Items().Append(winrt::box_value(L"Notify Only"));
        updateCombo.Items().Append(winrt::box_value(L"Silent Install"));
        updateCombo.SelectedIndex(Prefs.updater_mode_);
        panel.Children().Append(updateCombo);

        // Shutdown timing
        TextBlock timingLabel;
        timingLabel.Text(L"Shutdown timing:");
        panel.Children().Append(timingLabel);
        ComboBox timingCombo;
        timingCombo.Items().Append(winrt::box_value(L"Default"));
        timingCombo.Items().Append(winrt::box_value(L"Early"));
        timingCombo.Items().Append(winrt::box_value(L"Delayed"));
        timingCombo.SelectedIndex(Prefs.shutdown_timing_);
        panel.Children().Append(timingCombo);

        // Separator
        panel.Children().Append(MenuFlyoutSeparator());

        // User idle mode
        CheckBox idleCheck;
        idleCheck.Content(winrt::box_value(L"Enable user idle mode"));
        idleCheck.IsChecked(Prefs.user_idle_mode_);
        panel.Children().Append(idleCheck);

        // Remote streaming
        CheckBox remoteCheck;
        remoteCheck.Content(winrt::box_value(L"Support remote streaming hosts"));
        remoteCheck.IsChecked(Prefs.remote_streaming_host_support_);
        panel.Children().Append(remoteCheck);

        // Remote mode
        TextBlock remoteModeLabel;
        remoteModeLabel.Text(L"Remote streaming mode:");
        panel.Children().Append(remoteModeLabel);
        ComboBox remoteModeCombo;
        remoteModeCombo.Items().Append(winrt::box_value(L"Display off"));
        remoteModeCombo.Items().Append(winrt::box_value(L"Display blanked"));
        remoteModeCombo.SelectedIndex(Prefs.remote_streaming_host_prefer_power_off_ ? 0 : 1);
        remoteModeCombo.IsEnabled(Prefs.remote_streaming_host_support_);
        panel.Children().Append(remoteModeCombo);

        // Topology support
        CheckBox topologyCheck;
        topologyCheck.Content(winrt::box_value(L"Support multi-monitor topology"));
        topologyCheck.IsChecked(Prefs.topology_support_);
        panel.Children().Append(topologyCheck);

        // Topology on boot
        CheckBox topologyBootCheck;
        topologyBootCheck.Content(winrt::box_value(L"Restore topology at logon screen"));
        topologyBootCheck.IsChecked(Prefs.topology_keep_on_boot_);
        topologyBootCheck.IsEnabled(Prefs.topology_support_);
        panel.Children().Append(topologyBootCheck);

        // External API
        CheckBox apiCheck;
        apiCheck.Content(winrt::box_value(L"Enable external API"));
        apiCheck.IsChecked(Prefs.external_api_support_);
        panel.Children().Append(apiCheck);

        // Build dialog
        ContentDialog dialog;
        dialog.XamlRoot(this->Content().XamlRoot());
        dialog.Title(winrt::box_value(L"Global settings"));
        dialog.PrimaryButtonText(L"OK");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        ScrollViewer scrollViewer;
        scrollViewer.Content(panel);
        scrollViewer.MaxHeight(500);
        dialog.Content(scrollViewer);

        auto result = co_await dialog.ShowAsync();
        if (result == ContentDialogResult::Primary)
        {
            Prefs.power_on_timeout_ = static_cast<int>(timeoutBox.Value());
            Prefs.log_level_ = logCombo.SelectedIndex();

            int temp_updater_mode = updateCombo.SelectedIndex();
            if (temp_updater_mode != PREFS_UPDATER_OFF && Prefs.updater_mode_ == PREFS_UPDATER_OFF)
            {
                std::thread thread_obj([this]() { ThreadVersionCheck(); });
                thread_obj.detach();
            }
            Prefs.updater_mode_ = temp_updater_mode;

            Prefs.user_idle_mode_ = idleCheck.IsChecked().Value();
            Prefs.remote_streaming_host_support_ = remoteCheck.IsChecked().Value();
            Prefs.remote_streaming_host_prefer_power_off_ = remoteModeCombo.SelectedIndex() == 0;
            Prefs.topology_support_ = topologyCheck.IsChecked().Value();
            Prefs.topology_keep_on_boot_ = topologyBootCheck.IsChecked().Value();
            Prefs.external_api_support_ = apiCheck.IsChecked().Value();

            int selection_timing = timingCombo.SelectedIndex();
            if (selection_timing == 2)
                Prefs.shutdown_timing_ = PREFS_SHUTDOWN_TIMING_DELAYED;
            else if (selection_timing == 1)
                Prefs.shutdown_timing_ = PREFS_SHUTDOWN_TIMING_EARLY;
            else
                Prefs.shutdown_timing_ = PREFS_SHUTDOWN_TIMING_DEFAULT;

            ApplyButton().IsEnabled(true);
        }
    }
}
