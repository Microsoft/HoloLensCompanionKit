using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HoloLensCommander
{
    public class UserToken
    {
        public UserToken(string apiToken)
        {
            ApiToken = apiToken;
        }

        public string ApiToken { get; set; }
    }
}
