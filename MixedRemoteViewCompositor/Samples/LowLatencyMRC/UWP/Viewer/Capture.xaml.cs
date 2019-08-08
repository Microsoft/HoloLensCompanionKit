// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using MixedRemoteViewCompositor.Media;
using MixedRemoteViewCompositor.Network;

namespace Viewer
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class Capture : Page
    {
        private Listener listener = null;
        private Connection connection = null;
        private CaptureEngine captureEngine = null;

        public Capture()
        {
            this.InitializeComponent();
        }

        private async void bnListen_Click(object sender, RoutedEventArgs e)
        {
            if(this.listener != null)
            {
                CloseConnection();
            }

            ushort port = 0;
            if (UInt16.TryParse(txPort.Text, out port))
            {
                this.listener = new Listener(port);

                this.connection = await this.listener.ListenAsync();
                if (this.connection != null)
                {
                    this.connection.Disconnected += Connection_Disconnected;
                    this.connection.Received += Connection_Received;

                    StartCapture(cbEnableAudio.IsChecked.GetValueOrDefault(false));
                }
            }
        }

        private async void StartCapture(bool enableAudio)
        {
            captureEngine = await CaptureEngine.CreateAsync(enableAudio);
            if (this.captureEngine != null)
            {
                await this.captureEngine.StartAsync(false, this.connection);
            }
        }

        private void bnClose_Click(object sender, RoutedEventArgs e)
        {
            CloseConnection();
        }

        private void Connection_Disconnected(Connection sender)
        {
            CloseConnection();
        }

        private void Connection_Received(Connection sender, BundleReceivedArgs args)
        {

        }

        private async void CloseConnection()
        {
            if (this.captureEngine != null)
            {
                await this.captureEngine.StopAsync();
                this.captureEngine.Uninitialize();
                this.captureEngine = null;
            }

            if (this.connection != null)
            {
                this.connection.Disconnected -= Connection_Disconnected;
                this.connection.Received -= Connection_Received;
                this.connection.Uninitialize();
                this.connection = null;
            }

            if (this.listener != null)
            {
                this.listener.Uninitialize();
                this.listener = null;
            }
        }
    }
}
