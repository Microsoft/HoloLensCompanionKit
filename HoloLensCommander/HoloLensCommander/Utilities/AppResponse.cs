using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HoloLensCommander
{
    /// <summary>
    /// Class to create an object that saves app metadata for each application returned from enumerated JSON response.
    /// </summary>
    public class AppResponse
    {
        public string display_name;
        public string name;
        public string app_secret;
        public string id;
    }

    /// <summary>
    ///  Class to create an object that saves download URL for a specific application returned from enumerated JSON response.
    /// </summary>
    public class DownloadResponse
    {
        public string download_url;
    }
}
