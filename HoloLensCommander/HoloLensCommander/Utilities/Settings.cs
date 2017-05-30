﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HoloLensCommander
{
    public class Settings
    {
        // Constants used to set interval bounds and the default value
        public static readonly float DefaultHeartbeatInterval = 5.0f;
        public static readonly float MaxHeartbeatInterval = 60.0f;
        public static readonly float MinHeartbeatInterval = 1.0f;

        /// <summary>
        /// Indicates whether or not the application should reconnect to the previous
        /// device session.
        /// </summary>
        public bool AutoReconnect;

        /// <summary>
        /// Indicates whether or not the settings were successfully updated.
        /// </summary>
        /// <remarks>
        /// This value will be false if, for example, the heartbeat interval
        /// is out of bounds.
        /// </remarks>
        public bool SettingsUpdated = false;

        /// <summary>
        /// A message indicating the status of the settings object.
        /// </summary>
        /// <remarks>
        /// If the settings were not updated, this message should indicate why. 
        /// For example, "HeartbeatInterval must be between x and y, inclusive."
        /// </remarks>
        public string StatusMessage = string.Empty;

        /// <summary>
        /// The time, in seconds, beteen heartbeat checks.
        /// </summary>
        private float heartbeatInterval;
        public float HeartbeatInterval
        {
            get 
            {
                return this.heartbeatInterval;
            }

            set 
            {
                if (this.heartbeatInterval != value)
                {
                    if ((MaxHeartbeatInterval < value) ||
                        (MinHeartbeatInterval > value))
                    {
                        throw new Exception(
                            string.Format(
                            "Heartbeat Interval must be between {0:F1} and {1:F1}, inclusive.",
                            MinHeartbeatInterval,
                            MaxHeartbeatInterval));
                    }

                    this.heartbeatInterval = value;
                }
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="Settings" /> class.
        /// </summary>
        public Settings() :
            this(DefaultHeartbeatInterval)
        { }

        /// <summary>
        /// Initializes a new instance of the <see cref="Settings" /> class.
        /// </summary>
        /// <param name="interval">Seconds between heartbeat checks.</param>
        public Settings(float interval) : 
            this(false, interval)
        { }

        /// <summary>
        /// Initializes a new instance of the <see cref="Settings" /> class.
        /// </summary>
        /// <param name="autoReconnect">True to auto-reconnect, false otherwise.</param>
        /// <param name="interval">Seconds between heartbeat checks.</param>
        public Settings(
            bool autoReconnect,
            float interval)
        {
            this.AutoReconnect = autoReconnect;
            this.HeartbeatInterval = interval;
        }
    }
}
