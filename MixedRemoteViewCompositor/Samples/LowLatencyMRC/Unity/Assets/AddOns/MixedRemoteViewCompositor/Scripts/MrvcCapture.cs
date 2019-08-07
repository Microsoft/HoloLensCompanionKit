// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using UnityEngine;
using UnityEngine.XR.WSA;

namespace MixedRemoteViewCompositor
{
    public enum CaptureState
    {
        Idle,
        Initializing,
        Initialized,
        Starting,
        Started,
        Stopping,
        Stopped,
        Closing,
        Closed,
        Failed
    };

    [RequireComponent(typeof(Plugin))]
    public class MrvcCapture : MonoBehaviour
    {
        public bool EnableMRC = false;
        public bool StopOnPaused = false;

        public Action<object, StateChangedEventArgs<CaptureState>> CaptureStateChanged { get; internal set; }

        public CaptureState CaptureState
        {
            get { return this.captureState; }
            private set
            {
                if (this.captureState != value)
                {
                    this.previousCaptureState = this.captureState;
                    this.captureState = value;
                    this.plugin.QueueAction(() =>
                    {
                        if (this.CaptureStateChanged != null)
                        {
                            var args = new StateChangedEventArgs<CaptureState>(this.previousCaptureState, this.captureState, this.previousConnectionState, this.connectionState);
                            this.CaptureStateChanged(this, args);
                        }
                    });
                }
            }
        }
        private CaptureState captureState = CaptureState.Idle;
        private CaptureState previousCaptureState = CaptureState.Idle;

        public ConnectionState ConnectionState
        {
            get { return this.connectionState; }
            private set
            {
                if (this.connectionState != value)
                {
                    this.previousConnectionState = this.connectionState;
                    this.connectionState = value;
                    this.plugin.QueueAction(() =>
                    {
                        if (this.CaptureStateChanged != null)
                        {
                            var args = new StateChangedEventArgs<CaptureState>(this.previousCaptureState, this.captureState, this.previousConnectionState, this.connectionState);
                            this.CaptureStateChanged(this, args);
                        }
                    });
                }
            }
        }
        private ConnectionState connectionState = ConnectionState.Idle;
        private ConnectionState previousConnectionState = ConnectionState.Idle;

        private Plugin plugin = null;
        private Connection networkConnection = null;
        private CaptureEngine captureEngine = null;
        private IntPtr spatialCoordinateSystemPtr = IntPtr.Zero;

        private bool isPaused = false;
        private bool isStarted = false;

        private void Awake()
        {
            this.plugin = this.GetComponent<Plugin>();

            if(Plugin.IsHoloLens)
            {
                this.spatialCoordinateSystemPtr = WorldManager.GetNativeISpatialCoordinateSystemPtr();
            }
        }

        private void Start()
        {
            this.isStarted = true;

            // on HoloLens gets the instance of the spatial coordinate system, to be used in Mixed Media Rendering
            if (Plugin.IsHoloLens)
            {
                WorldManager.OnPositionalLocatorStateChanged += (oldState, newState) =>
                {
                    Debug.Log("WorldManager.OnPositionalLocatorStateChanged, updating any capture in progress");

                    this.spatialCoordinateSystemPtr = WorldManager.GetNativeISpatialCoordinateSystemPtr();
                    if (this.captureEngine != null)
                    {
                        this.captureEngine.SetSpatialCoordinateSystem(this.spatialCoordinateSystemPtr);
                    }
                };
            }
        }

        private void OnDisable()
        {
            this.Shutdown();
        }

        private void OnApplicationPause(bool pauseStatus)
        {
            this.isPaused = pauseStatus;

            if (!this.isStarted || !this.StopOnPaused)
            {
                return;
            }

            if (this.isPaused)
            {
                this.CaptureStop();
            }
            else
            {
                this.CaptureInitialize(this.networkConnection);
            }
        }

        public void Initialize(Connection connection)
        {
            if (connection == null)
            {
                Debug.LogError("MrvcCapture.Initialize() - requires a valid connection component to start.");

                return;
            }

            if (this.networkConnection != null || this.captureEngine != null)
            {
                Debug.LogError("MrvcCapture.Initialize() - cannot start until previous instance is stopped.");

                return;
            }

            this.ConnectionState = ConnectionState.Idle;
            this.CaptureState = CaptureState.Idle;

            this.CaptureInitialize(connection);
        }
        public void Shutdown()
        {
            if (this.captureEngine != null && this.CaptureState != CaptureState.Failed)
            {
                this.CaptureStop();
            }
            else
            {
                this.ConnectionClose();
            }
        }

        private void CaptureInitialize(Connection connection)
        {
            if (connection == null)
            {
                return;
            }

            this.networkConnection = connection;
            this.networkConnection.Disconnected += this.OnDisconnected;
            this.networkConnection.Closed += this.OnConnectionClosed;
            this.ConnectionState = ConnectionState.Connected;

            if (CaptureEngine.Create(this.OnCaptureInitialized))
            {
                this.CaptureState = CaptureState.Initializing;
            }
            else
            {
                this.CaptureState = CaptureState.Failed;

                Shutdown();
            }
        }
        private void CaptureInitialized(CaptureEngine engine)
        {
            this.captureEngine = engine;
            if (this.captureEngine != null)
            {
                this.CaptureStart();
            }

            if (this.captureState != CaptureState.Starting)
            {
                this.CaptureState = CaptureState.Failed;

                this.Shutdown();
            }
        }

        private void CaptureStart()
        {
            if (this.captureEngine == null)
            {
                return;
            }

            this.captureEngine.Started += this.OnCaptureStarted;
            this.captureEngine.Stopped += this.OnCaptureStopped;
            this.captureEngine.Closed += this.OnCaptureClosed;
            this.captureEngine.Failed += this.OnCaptureFailed;
            this.captureEngine.Start(this.networkConnection, this.EnableMRC, this.spatialCoordinateSystemPtr);

            this.CaptureState = CaptureState.Starting;
        }
        private void CaptureStarted()
        {
            if (this.captureState != CaptureState.Started)
            {
                this.CaptureState = CaptureState.Failed;

                Shutdown();
            }
        }

        private void CaptureStop()
        {
            if (this.captureEngine != null)
            {
                this.CaptureState = CaptureState.Stopping;

                this.captureEngine.Stop();
            }

            if (this.captureState != CaptureState.Stopping)
            {
                this.CaptureState = CaptureState.Failed;

                Shutdown();
            }
        }
        private void CaptureStopped()
        {
            CaptureClose();

            if (this.captureState != CaptureState.Closing)
            {
                this.CaptureState = CaptureState.Failed;

                Shutdown();
            }
        }

        private void CaptureClose()
        {
            if (this.captureEngine != null)
            {
                this.CaptureState = CaptureState.Closing;

                this.captureEngine.Started -= this.OnCaptureStarted;
                this.captureEngine.Stopped -= this.OnCaptureStopped;
                this.captureEngine.Failed -= this.OnCaptureFailed;
                this.captureEngine.Close();
            }
        }
        private void CaptureClosed()
        {
            if (this.captureEngine != null)
            {
                this.captureEngine.Closed -= this.OnCaptureClosed;
                this.captureEngine.Dispose();
            }
            this.captureEngine = null;

            if (this.captureState != CaptureState.Closed)
            {
                this.CaptureState = CaptureState.Failed;
            }

            Shutdown();
        }

        private void ConnectionClose()
        {
            if (this.networkConnection == null)
            {
                return;
            }

            this.ConnectionState = ConnectionState.Closing;

            this.networkConnection.Close();
        }
        private void ConnectionClosed()
        {
            if (this.networkConnection == null)
            {
                return;
            }

            this.networkConnection.Closed -= this.OnConnectionClosed;
            this.networkConnection.Dispose();
            this.networkConnection = null;

            this.ConnectionState = ConnectionState.Closed;
        }

        private void OnCaptureInitialized(CaptureEngine engine)
        {
            this.plugin.QueueAction(() =>
            {
                this.CaptureState = CaptureState.Initialized;

                CaptureInitialized(engine);
            });
        }
        private void OnCaptureStarted(object sender, EventArgs e)
        {
            this.plugin.QueueAction(() =>
            {
                this.CaptureState = CaptureState.Started;

                this.CaptureStarted();
            });
        }
        private void OnCaptureStopped(object sender, EventArgs e)
        {
            this.plugin.QueueAction(() =>
            {
                this.CaptureState = CaptureState.Stopped;

                this.CaptureStopped();
            });
        }
        private void OnCaptureClosed(object sender, EventArgs e)
        {
            this.plugin.QueueAction(() =>
            {
                this.CaptureState = CaptureState.Closed;

                this.CaptureClosed();
            });
        }
        private void OnCaptureFailed(object sender, FailedEventArgs args)
        {
            this.plugin.QueueAction(() =>
            {
                Debug.LogWarningFormat("MrvcCapture failed: {0} - {1}", args.Result, args.Message);

                this.CaptureState = CaptureState.Failed;

                this.Shutdown();
            });
        }

        private void OnDisconnected(object sender, EventArgs e)
        {
            this.plugin.QueueAction(() =>
            {
                this.ConnectionState = ConnectionState.Disconnected;

                this.Shutdown();
            });
        }
        private void OnConnectionClosed(object sender, EventArgs eventArgs)
        {
            this.plugin.QueueAction(() =>
            {
                this.ConnectionState = ConnectionState.Closed;

                ConnectionClosed();
            });
        }
        private void OnConnectionFailed(object sender, EventArgs e)
        {
            this.plugin.QueueAction(() =>
            {
                this.ConnectionState = ConnectionState.Failed;

                this.Shutdown();
            });
        }
    }
}
