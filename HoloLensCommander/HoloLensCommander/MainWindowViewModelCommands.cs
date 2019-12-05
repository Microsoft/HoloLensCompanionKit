// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Threading.Tasks;
using System.Windows.Input;
using System.Xml.Serialization;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.UI.Popups;
using Windows.UI.Xaml.Controls;

namespace HoloLensCommander
{
    /// <summary>
    /// The view model for the MainPage.
    /// </summary>
    partial class MainWindowViewModel
    {
        /// <summary>
        /// The identifiers of the connect context menu commands.
        /// </summary>
        private enum ConnectContextMenuCommandIds
        {
            ReconnectPreviousSession = 0,
            SetDefaultCredentials,
            UseDefaultCredentials
        }

        /// <summary>
        /// Command used to clear the delected devices status messages.
        /// </summary>
        public ICommand ClearDeviceStatusCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the clear device status command.
        /// </summary>
        private async Task ClearDeviceStatus()
        {
            YesNoMessageDialog messageDialog = new YesNoMessageDialog(
                "Are you sure you want to clear the status for the selected devices?");
            if (MessageDialogButtonId.Yes != await messageDialog.ShowAsync())
            {
                return;
            }

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                monitor.ClearStatusMessage();
            }
        }

        /// <summary>
        /// Command used to clear the application status message.
        /// </summary>
        public ICommand ClearStatusMessageCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the clear status message command.
        /// </summary>
        private void ClearStatusMessage()
        {
            this.StatusMessage = string.Empty;
        }
        
        /// <summary>
        /// Command used to close all applications on the selected devices.
        /// </summary>
        public ICommand CloseAllAppsCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the close all applications command.
        /// </summary>
        private void CloseAllApps()
        {
            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                // Assigning the return value of CloseAllAppsAsync to a Task object to avoid 
                // warning 4014 (call is not awaited).
                Task t = monitor.CloseAllAppsAsync();
            }

            this.StatusMessage = string.Empty;
        }

        /// <summary>
        /// Command used to connect to a device.
        /// </summary>
        public ICommand ConnectToDeviceCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the connect to device command.
        /// </summary>
        /// <param name="connectOptions">Options used when connecting to the device.</param>
        /// <param name="name">Descriptive name to assign to the device.</param>
        /// <param name="suppressDialog">True causes the connection dialog to not be shown./param>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task ConnectToDeviceAsync(
            ConnectOptions connectOptions,
            bool suppressDialog = false)
        {
            this.StatusMessage = string.Empty;

            if (!suppressDialog)
            {
                ContentDialog dialog = new ConnectToDeviceDialog(connectOptions);
                ContentDialogResult result = await dialog.ShowAsync().AsTask<ContentDialogResult>();

                // Primary button == "Ok"
                if (result != ContentDialogResult.Primary)
                {
                    // The user decided not to connect.
                    return;
                }
            }

            if (string.IsNullOrWhiteSpace(connectOptions.Address))
            {
                connectOptions.Address = DeviceMonitor.DefaultConnectionAddress;
            }

            DeviceMonitor monitor = new DeviceMonitor(this.dispatcher);

            this.StatusMessage = string.Format(
                "Connecting to the device at {0}",
                connectOptions.Address);

            await monitor.ConnectAsync(connectOptions);
            this.StatusMessage = string.Empty;

            await this.RegisterDeviceAsync(
                monitor, 
                connectOptions.Name);
        }

        /// <summary>
        /// Command used to deselect all of the registered devices.
        /// </summary>
        public ICommand DeselectAllDevicesCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the deselect all devices command.
        /// </summary>
        private void DeselectAllDevices()
        {
            // Suppress the refresh call that occurs in the SelectionChanged event handler.
            this.suppressRefreshCommonApps = true;

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel monitorViewModel = monitor.ViewModel;
                if ((monitorViewModel.Filter & this.SelectionFilter) == monitorViewModel.Filter)
                {
                    monitorViewModel.IsSelected = false;
                }
            }

            // Assigning the return value of RefreshCommonAppsAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.RefreshCommonAppsAsync();

            // Restore the nominal behavior of the SelectionChanged event handler.
            this.suppressRefreshCommonApps = false;
        }

        /// <summary>
        /// Command used to forget registered devices.
        /// </summary>
        public ICommand ForgetConnectionsCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the forget connections command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task ForgetAllConnectionsAsync()
        {
            YesNoMessageDialog messageDialog = new YesNoMessageDialog(
                "Are you sure you want to unregister the selected devices?");
            if (MessageDialogButtonId.Yes != await messageDialog.ShowAsync())
            {
                return;
            }

            this.suppressSave = true;

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel viewModel = (DeviceMonitorControlViewModel)monitor.DataContext;
                if (viewModel.IsSelected)
                {
                    monitor.Disconnect();
                    this.RegisteredDevices.Remove(monitor);
                }
            }

            this.suppressSave = false;

            await this.SaveConnectionsAsync();

            this.StatusMessage = string.Empty;
        }

        /// <summary>
        /// Command used to install an application on the selected devices.
        /// </summary>
        public ICommand InstallAppCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the install application command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task InstallAppAsync()
        {
            // Prompt the user for the required file.
            AppInstallFiles installFiles = new AppInstallFiles();
            ContentDialog dialog = new GetAppInstallFilesDialog(installFiles);
            await dialog.ShowAsync();
            
            if (installFiles.AppPackageFile == null)
            {
                return;
            }

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel viewModel = monitor.ViewModel;
                if (!viewModel.IsSelected) { continue; }

                // Assigning the return value of InstallAppAsync to a Task object to avoid 
                // warning 4014 (call is not awaited).
                Task t = monitor.InstallAppAsync(installFiles);
            }
        }

        /// <summary>
        /// Command used to launch an application on the selected devices.
        /// </summary>
        public ICommand LaunchAppCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the launch application command.
        /// </summary>
        private void LaunchApp()
        {
            string appName = this.SelectedApp as string;

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                // Assigning the return value of LaunchAppAsync to a Task object to avoid 
                // warning 4014 (call is not awaited).
                Task t = monitor.LaunchAppAsync(appName);
            }
        }

        public ICommand LoadSessionFileCommand
        { get; private set; }

        private async Task LoadSessionFileAsync()
        {
            List<ConnectionInformation> devices = new List<ConnectionInformation>();

            FileOpenPicker filePicker = new FileOpenPicker();
            filePicker.FileTypeFilter.Add(".xml");
            filePicker.CommitButtonText = "Select";

            StorageFile file = await filePicker.PickSingleFileAsync();
            if (file == null)
            {
                return;
            }

            this.StatusMessage = string.Format(
                "Loading session file: {0}",
                file.Path);

            this.RegisteredDevices.Clear();

            // Assigning the return value of ReconnectToDevicesAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.ReconnectToDevicesAsync(file);
        }

        /// <summary>
        /// Command used to reboot the selected devices.
        /// </summary>
        public ICommand RebootDevicesCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the reboot devices command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task RebootDevicesAsync()
        {
            YesNoMessageDialog messageDialog = new YesNoMessageDialog(
                "Are you sure you want to reboot the selected devices?");
            if (MessageDialogButtonId.Yes != await messageDialog.ShowAsync())
            {
                return;
            }

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                // Assigning the return value of RebootAsync to a Task object to avoid 
                // warning 4014 (call is not awaited).
                Task t = monitor.RebootAsync();
            }
        }

        /// <summary>
        /// Implementation of the reconnect to devices command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task ReconnectToDevicesAsync(
            StorageFile sessionFile = null)
        {
            // TODO: allow the user to cancel this if it is taking too long

            this.reconnected = true;
            this.suppressSave = true;

            // Defer updating common apps until all devices have reconnected
            this.suppressRefreshCommonApps = true;

            List<ConnectionInformation> connections = await this.ReadSessionFileAync(sessionFile);

            foreach (ConnectionInformation connectionInfo in connections)
            {
                ConnectOptions connectOptions = new ConnectOptions(
                    connectionInfo.Address,
                    connectionInfo.Name,
                    this.UserName,
                    this.Password);
                connectOptions.UseInstalledCertificate = this.useInstalledCertificate;
                // Since we are suppressing the UI, we do not need to provide
                // values for ExpandCredentials or ExpandNetworkSettings.

                // We assume that since we are reconnecting, the device has previously
                // been connected to the correct network access point.
                // Therefore, we are omitting the Ssid and NetworkKey values.

                await this.ConnectToDeviceAsync(
                    connectOptions,
                    true); // Do not show the connect dialog on re-connect.
            }

            this.suppressRefreshCommonApps = false;
            this.suppressSave = false;

            // All known devices have been reconnected, refresh common apps now.
            await this.RefreshCommonAppsAsync();

            this.UpdateCanReconnect();
        }

        /// <summary>
        /// Command used to restore device connections from a previous application session.
        /// </summary>
        public ICommand ReconnectPreviousSessionCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the reconnect previous session command.
        /// </summary>
        private void ReconnectPreviousSession()
        {
            // Assigning the return value of ReconnectToDevicesAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = ReconnectToDevicesAsync();
        }

        /// <summary>
        /// Command used to refresh the list of applications that are installed on all of on the selected devices.
        /// </summary>
        public ICommand RefreshCommonAppsCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the refresh common applications command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task RefreshCommonAppsAsync()
        {
            // Early exit if refresh has been suppressed.
            if (this.suppressRefreshCommonApps) { return; }

            this.StatusMessage = "Refreshing common applications";

            List<string> commonAppNames = new List<string>();

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel viewModel = monitor.ViewModel;
                if (!viewModel.IsSelected) { continue; }

                List<string> deviceAppNames = await monitor.GetInstalledAppNamesAsync();

                // If this is the first device queried...
                if (commonAppNames.Count == 0)
                {
                    // ... Add all apps.
                    commonAppNames.AddRange(deviceAppNames);
                }
                else
                {
                    List<string> appNamesToRemove = new List<string>();

                    // Remove apps that do not exist on this device.
                    foreach (string name in commonAppNames)
                    {
                        if (!deviceAppNames.Contains(name))
                        {
                            appNamesToRemove.Add(name);
                        }
                    }

                    foreach (string name in appNamesToRemove)
                    {
                        commonAppNames.Remove(name);
                    }
                }
            }

            this.UpdateCommonAppsCollection(commonAppNames);

            this.StatusMessage = string.Empty;
        }

        /// <summary>
        /// Command used to save mixed reality files.
        /// </summary>
        public ICommand SaveMixedRealityFilesCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the save mixed reality files command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task SaveMixedRealityFiles()
        {
            // We save images and videos in a subfolder of the pictures library.
            StorageFolder picturesLibrary = KnownFolders.PicturesLibrary; 
            StorageFolder folder = await picturesLibrary.CreateFolderAsync(
                MixedRealityFilesFolderName,
                CreationCollisionOption.OpenIfExists);

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel monitorViewModel = monitor.ViewModel;

                if ((monitorViewModel.Filter & DeviceFilters.HoloLens) == DeviceFilters.HoloLens)
                {
                    await monitor.GetMixedRealityFilesAsync(
                        folder,
                        this.DeleteMixedRealityFilesAfterSave);
                }
            }
        }

        /// <summary>
        /// Command used to save the current device session.
        /// </summary>
        public ICommand SaveSessionFileCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the save session file command.
        /// </summary>
        private async Task SaveSessionFile()
        {
            this.ClearStatusMessage();

            try
            {
                // Use the picker to select the target location.
                FileSavePicker savePicker = new FileSavePicker();
                savePicker.FileTypeChoices.Add("Windows Mixed Reality Commander session file", new List<String>() { ".xml" });
                savePicker.DefaultFileExtension = ".xml";

                StorageFile file = await savePicker.PickSaveFileAsync();
                if (file == null) { return; }

                List<ConnectionInformation> sessionDevices = new List<ConnectionInformation>();
                foreach (DeviceMonitorControl monitorControl in this.GetCopyOfRegisteredDevices())
                {
                    DeviceMonitorControlViewModel viewModel = (DeviceMonitorControlViewModel)monitorControl.DataContext;
                    sessionDevices.Add(
                        new ConnectionInformation(
                        viewModel.Address,
                        viewModel.Name));
                }

                // Serializing to a memory stream and writing the file using the FileIO class to ensure
                // that, if overwriting an existing file, all previous contents are removed.
                using (MemoryStream stream = new MemoryStream())
                {
                    XmlSerializer serializer = new XmlSerializer(typeof(List<ConnectionInformation>));
                    serializer.Serialize(stream, sessionDevices);
                    stream.Position = 0;

                    using (StreamReader reader = new StreamReader(stream))
                    {
                        string data = await reader.ReadToEndAsync();

                        await FileIO.WriteTextAsync(file, data);
                    }
                }

                this.StatusMessage = string.Format(
                    "Session file saved as {0}",
                    file.Path);
            }
            catch
            {
                this.StatusMessage = "Failed to save the session file.";
            }
        }

        /// <summary>
        /// Command used to select all of the the registered devices.
        /// </summary>
        public ICommand SelectAllDevicesCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the select all devices command.
        /// </summary>
        private void SelectAllDevices()
        {
            // Suppress the refresh call that occurs in the SelectionChanged event handler.
            this.suppressRefreshCommonApps = true;

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel monitorViewModel = monitor.ViewModel;
                if ((monitorViewModel.Filter & this.SelectionFilter) == monitorViewModel.Filter)
                {
                    monitorViewModel.IsSelected = true;
                }
            }

            // Assigning the return value of RefreshCommonAppsAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.RefreshCommonAppsAsync();

            // Restore the nominal behavior of the SelectionChanged event handler.
            this.suppressRefreshCommonApps = false;
        }

        /// <summary>
        /// Command used to display the set credentials dialog.
        /// </summary>
        public ICommand ShowSetCredentialsCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the show set credentials command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task ShowSetCredentials()
        {
            NetworkCredential credentials = new NetworkCredential(
                this.UserName, 
                this.Password);

            SetCredentialsDialog credsDialogs = new SetCredentialsDialog(credentials);
            ContentDialogResult dialogResult = await credsDialogs.ShowAsync();
            
            // Was the Ok button clicked?
            if (dialogResult == ContentDialogResult.Primary)
            {
                this.UserName = credentials.UserName;
                this.Password = credentials.Password;
                this.SaveApplicationSettings();
            }
        }

        public ICommand ShowMobileCenterAppsCommand
        { get; private set; }

        private async Task ShowMobileCenterApps()
        {
            AppCenterWebRequest appCenterWebRequest = new AppCenterWebRequest();
            string requestResponseJson = await appCenterWebRequest.HttpWebRequest();

            AppResponse[] appResponse;
            appResponse = JsonConvert.DeserializeObject<AppResponse[]>(requestResponseJson);

            ContentDialog mobileCenterAppsDialog = new MobileCenterAppsDialog(appResponse);
            await mobileCenterAppsDialog.ShowAsync();
        }

        public ICommand ShowSetAPItokenCommand
        { get; private set; }

        private async Task ShowSetAPItoken()
        {
            UserToken apiToken = new UserToken(this.ApiToken);

            SetAPItokenDialog apiTokenDialogs = new SetAPItokenDialog(apiToken);
            ContentDialogResult dialogResult = await apiTokenDialogs.ShowAsync();

            if (dialogResult==ContentDialogResult.Primary)
            {
                this.ApiToken = apiToken.ApiToken;
                this.SaveApplicationSettings();
            }
        }

        /// <summary>
        /// Command used to display the settings dialog.
        /// </summary>
        public ICommand ShowSettingsCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the show settings command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task ShowSettings()
        {
            Settings settings = new Settings(
                this.autoReconnect,
                this.heartbeatInterval,
                this.expandCredentials,
                this.expandNetworkSettings,
                this.useInstalledCertificate,
                this.defaultSsid,
                this.defaultNetworkKey);

            SettingsDialog settingsDialog = new SettingsDialog(settings);
            ContentDialogResult dialogResult = await settingsDialog.ShowAsync();

            // Was the Ok button clicked?
            if (dialogResult == ContentDialogResult.Primary)
            {
                if (!settings.SettingsUpdated)
                {
                    MessageDialog errorMessage = new MessageDialog(
                        string.Format(
                        "An error occurred updating settings:\r\n\r\n{0}",
                        settings.StatusMessage));
                    await errorMessage.ShowAsync();
                    return;
                }

                this.autoReconnect = settings.AutoReconnect;
                this.expandCredentials = settings.ExpandCredentials;
                this.expandNetworkSettings = settings.ExpandNetworkSettings;
                this.heartbeatInterval = settings.HeartbeatInterval;
                this.useInstalledCertificate = settings.UseInstalledCertificate;
                this.defaultSsid = settings.DefaultSsid;
                this.defaultNetworkKey = settings.DefaultNetworkKey;
                this.SaveApplicationSettings();

                // Update the device monitors with the new heartbeat interval.
                List<DeviceMonitorControl> registeredDevices = this.GetCopyOfRegisteredDevices();
                foreach (DeviceMonitorControl monitor in registeredDevices)
                {
                    ((DeviceMonitorControlViewModel)monitor.DataContext).SetHeartbeatInterval(this.heartbeatInterval);
                }
            }
        }

        /// <summary>
        /// Command used to shutdown the selected devices.
        /// </summary>
        public ICommand ShutdownDevicesCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the shutdown devices command.
        /// </summary>
        private async Task ShutdownDevicesAsync()
        {
            YesNoMessageDialog messageDialog = new YesNoMessageDialog(
                "Are you sure you want to shutdown the selected devices?");
            if (MessageDialogButtonId.Yes != await messageDialog.ShowAsync())
            {
                return;
            }

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                // Assigning the return value of ShutdownAsync to a Task object to avoid 
                // warning 4014 (call is not awaited).
                Task t = monitor.ShutdownAsync();
            }
        }

        /// <summary>
        /// Command used to start recording a mixed reality video on the selected devices.
        /// </summary>
        public ICommand StartMixedRealityRecordingCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the start mixed reality recording command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private void StartMixedRealityRecording()
        {
            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel monitorViewModel = monitor.ViewModel;

                if ((monitorViewModel.Filter & DeviceFilters.HoloLens) == DeviceFilters.HoloLens)
                {
                    // Assigning the return value of StartMixedRealityRecordingAsync 
                    // to a Task object to avoid warning 4014 (call is not awaited).
                    Task t = monitor.StartMixedRealityRecordingAsync();
                }
            }
        }

        /// <summary>
        /// Command used to stop the mixed reality recording on the selected devices.
        /// </summary>
        public ICommand StopMixedRealityRecordingCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the stop mixed reality recording command.
        /// </summary>
        private void StopMixedRealityRecording()
        {
            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel monitorViewModel = monitor.ViewModel;

                if ((monitorViewModel.Filter & DeviceFilters.HoloLens) == DeviceFilters.HoloLens)
                {
                    // Assigning the return value of StopMixedRealityRecordingAsync 
                    // to a Task object to avoid warning 4014 (call is not awaited).
                    Task t = monitor.StopMixedRealityRecordingAsync();
                }                
            }
        }   
            
        /// <summary>
        /// Command used to uninstall an application on the selected devices.
        /// </summary>
        public ICommand UninstallAppCommand
        { get; private set; }

        /// <summary>
        /// Implementation of the uninstall application command.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private void UninstallApp()
        {
            string appName = this.SelectedApp as string;

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                // Assigning the return value of UninstallAppAsync to a Task object to avoid 
                // warning 4014 (call is not awaited).
                Task t = monitor.UninstallAppAsync(appName);
            }
        }

        /// <summary>
        /// Command used to uninstall all side loaded applications on selected devices
        /// </summary>
        public ICommand UninstallAllAppsCommand
        { get; private set; }

        /// <summary>
        /// Implementation of uninstall all apps command.
        /// </summary>
        private void UninstallAllApps()
        {
            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                Task t = monitor.UninstallAllAppsAsync();
            }
        }

        /// <summary>
        /// Command indicating that the select all/none buttons should use the all devices filter.
        /// </summary>
        public ICommand UseAllDevicesFilterCommand
        { get; private set; }

        /// <summary>
        /// Command indicating that the select all/none buttons should use the desktop filter.
        /// </summary>
        public ICommand UseDesktopFilterCommand
        { get; private set; }

        /// <summary>
        /// Command indicating that the select all/none buttons should use the HoloLens filter.
        /// </summary>
        public ICommand UseHoloLensFilterCommand
        { get; private set; }

        /// <summary>
        /// Command that wipes all camera roll content on selected devices
        /// </summary>
        public ICommand WipeCameraRollCommand
        { get; private set; }

        /// <summary>
        /// Implementation of wipe camera roll command
        /// </summary>
        private async void WipeCameraRoll()
        {
            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel monitorViewModel = monitor.ViewModel;

                if ((monitorViewModel.Filter & DeviceFilters.HoloLens) == DeviceFilters.HoloLens)
                {
                    await monitor.WipeCameraRollAsync();
                }
            }
        }

        /// <summary>
        /// Handles the AppInstalled event.
        /// </summary>
        /// <param name="sender">The object which sent this event.</param>
        private void DeviceMonitorControl_AppInstalled(DeviceMonitorControl sender)
        {
            // Assigning the return value of RefreshCommonAppsAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.RefreshCommonAppsAsync();
        }

        /// <summary>
        /// Handles the AppUninstalled event.
        /// </summary>
        /// <param name="sender">The object which sent this event.</param>
        private void DeviceMonitorControl_AppUninstalled(DeviceMonitorControl sender)
        {
            // Assigning the return value of RefreshCommonAppsAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.RefreshCommonAppsAsync();
        }

        /// <summary>
        /// Handles the Disconnected event.
        /// </summary>
        /// <param name="sender">The object which sent this event.</param>
        private void DeviceMonitorControl_Disconnected(DeviceMonitorControl sender)
        {
            // Assigning the return value of UnregisterDeviceAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.UnregisterDeviceAsync(sender);
        }

        /// <summary>
        /// Handles the SelectedChanged event.
        /// </summary>
        /// <param name="sender">The object which sent this event.</param>
        private void DeviceMonitorControl_SelectedChanged(DeviceMonitorControl sender)
        {
            if (this.suppressRefreshCommonApps) { return; }

            // Assigning the return value of UnregisterDeviceAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.RefreshCommonAppsAsync();
        }

        /// <summary>
        /// Handles the TagChanged event.
        /// </summary>
        /// <param name="sender">The object which sent this event.</param>
        private void DeviceMonitorControl_TagChanged(DeviceMonitorControl sender)
        {
            // Assigning the return value of SaveConnectionsAsync to a Task object to avoid 
            // warning 4014 (call is not awaited).
            Task t = this.SaveConnectionsAsync();
        }

        /// <summary>
        /// Reads the application settings.
        /// </summary>
        private void LoadApplicationSettings()
        {
            this.UserName = this.appSettings.Values[DefaultUserNameKey] as string;
            this.Password = this.appSettings.Values[DefaultPasswordKey] as string;

            this.ApiToken = this.appSettings.Values[DefaultAPItokenKey] as string;
            this.AppCenterUserName = this.appSettings.Values[DefaultAppCenterUserNameKey] as string;
            this.AppCenterPassword = this.appSettings.Values[DefaultAppCenterPasswordKey] as string;

            this.defaultSsid = this.appSettings.Values[DefaultSsidKey] as string;
            this.defaultNetworkKey = this.appSettings.Values[DefaultNetworkKeyKey] as string;

            if (!bool.TryParse(
                this.appSettings.Values[AutoReconnectKey] as string, 
                out this.autoReconnect))
            {
                this.autoReconnect = false;
            }

            if (!bool.TryParse(
                this.appSettings.Values[ExpandCredentialsKey] as string,
                out this.expandCredentials))
            {
                this.expandCredentials = false;
            }

            if (!bool.TryParse(
                this.appSettings.Values[ExpandNetworkSettingsKey] as string,
                out this.expandNetworkSettings))
            {
                this.expandNetworkSettings = false;
            }

            if (!float.TryParse(
                this.appSettings.Values[HeartbeatIntervalKey] as string,
                out this.heartbeatInterval))
            {
                this.heartbeatInterval = Settings.DefaultHeartbeatInterval;
            }

            if (!bool.TryParse(
                this.appSettings.Values[UseInstalledCertificateKey] as string,
                out this.useInstalledCertificate))
            {
                this.useInstalledCertificate = false;
            }
        }

        /// <summary>
        /// Read saved device connections from disk.
        /// </summary>
        /// <param name="sessionFile">StorageFile containing the connection information.</param>
        /// <returns>List of connection information (address, name, etc).</returns>
        private async Task<List<ConnectionInformation>> ReadSessionFileAync(
            StorageFile sessionFile = null)
        {
            // NOTE: When we load a connection, we assume that we are to use the
            // default credentials. If our connection fails, we will skip it.

            List<ConnectionInformation> connections = null;

            try
            {
                if (sessionFile == null)
                {
                    sessionFile = await this.localFolder.GetFileAsync(
                        MainPage.ConnectionsFileName);
                }

                using (Stream stream = await sessionFile.OpenStreamForReadAsync())
                {
                    XmlSerializer serializer = new XmlSerializer(typeof(List<ConnectionInformation>));
                    connections = serializer.Deserialize(stream) as List<ConnectionInformation>;
                }
            }
            catch
            {
                connections = new List<ConnectionInformation>();
            }

            return connections;
        }

        /// <summary>
        /// Registers the DeviceMonitor with the application.
        /// </summary>
        /// <param name="monitor">Device to register.</param>
        /// <param name="name">Descriptive name of the device.</param>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task RegisterDeviceAsync(
            DeviceMonitor monitor,
            string name)
        {
            DeviceMonitorControl deviceMonitorControl = new DeviceMonitorControl(monitor);
            deviceMonitorControl.AppInstalled += DeviceMonitorControl_AppInstalled;
            deviceMonitorControl.AppUninstalled += DeviceMonitorControl_AppUninstalled;
            deviceMonitorControl.DeviceDisconnected += DeviceMonitorControl_Disconnected;
            deviceMonitorControl.SelectedChanged += DeviceMonitorControl_SelectedChanged;
            deviceMonitorControl.TagChanged += DeviceMonitorControl_TagChanged;

            DeviceMonitorControlViewModel viewModel = deviceMonitorControl.DataContext as DeviceMonitorControlViewModel;
            viewModel.Name = name;

            // We want a sorted list of devices.
            List<DeviceMonitorControl> currentDevices = this.GetCopyOfRegisteredDevices();
            currentDevices.Add(deviceMonitorControl);
            currentDevices.Sort(new DeviceMonitorControlComparer());

            this.RegisteredDevices.Clear();
            foreach (DeviceMonitorControl monitorControl in currentDevices)
            {
                this.RegisteredDevices.Add(monitorControl);
            }
            if (this.RegisteredDevices.Count > 0)
            {
                this.HaveRegisteredDevices = true;
            }

            currentDevices.Clear();
            currentDevices = null;

            await this.SaveConnectionsAsync();
        }

        /// <summary>
        /// Saves the application settings.
        /// </summary>
        private void SaveApplicationSettings()
        {
            this.appSettings.Values[DefaultUserNameKey] = this.UserName;
            this.appSettings.Values[DefaultPasswordKey] = this.Password;

            this.appSettings.Values[DefaultAPItokenKey] = this.ApiToken;
            this.appSettings.Values[DefaultAppCenterUserNameKey] = this.AppCenterUserName;
            this.appSettings.Values[DefaultAppCenterPasswordKey] = this.AppCenterPassword;

            this.appSettings.Values[DefaultSsidKey] = this.defaultSsid;
            this.appSettings.Values[DefaultNetworkKeyKey] = this.defaultNetworkKey;

            this.appSettings.Values[AutoReconnectKey] = this.autoReconnect.ToString();
            this.appSettings.Values[ExpandCredentialsKey] = this.expandCredentials.ToString();
            this.appSettings.Values[ExpandNetworkSettingsKey] = this.expandNetworkSettings.ToString();
            this.appSettings.Values[HeartbeatIntervalKey] = this.heartbeatInterval.ToString();
            this.appSettings.Values[UseInstalledCertificateKey] = this.useInstalledCertificate.ToString();
        }

        /// <summary>
        /// Saves device connections to disk.
        /// </summary>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task SaveConnectionsAsync()
        {
            if (this.suppressSave)
            {
                return;
            }

            List<ConnectionInformation> connections = new List<ConnectionInformation>();

            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                DeviceMonitorControlViewModel monitorViewModel = monitor.ViewModel;

                connections.Add(new ConnectionInformation(
                    monitorViewModel.Address,
                    monitorViewModel.Name));
            }

            try 
            {
                StorageFile connectionsFile = await this.localFolder.CreateFileAsync(
                    MainPage.ConnectionsFileName,
                    CreationCollisionOption.ReplaceExisting);

                using (Stream stream = await connectionsFile.OpenStreamForWriteAsync())
                {
                    XmlSerializer serializer = new XmlSerializer(typeof(List<ConnectionInformation>));
                    serializer.Serialize(stream, connections);
                    await stream.FlushAsync();
                }
            }
            catch
            {
                this.StatusMessage = "Failed to save connection information";
            }
        }

        /// <summary>
        /// Removes the specified device from monitoring.
        /// </summary>
        /// <param name="monitorControl">The DeviceMonitorControl tracking the device to be removed.</param>
        /// <returns>Task object used for tracking method completion.</returns>
        private async Task UnregisterDeviceAsync(DeviceMonitorControl monitorControl)
        {
            this.RegisteredDevices.Remove(monitorControl);
            if (this.RegisteredDevices.Count == 0)
            {
                this.HaveRegisteredDevices = false;
            }

            await this.SaveConnectionsAsync();
        }
    }
}
