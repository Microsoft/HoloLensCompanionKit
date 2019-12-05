using System;
using System.IO.Compression;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Networking.BackgroundTransfer;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.Web.Http;
using Windows.Web.Http.Headers;

namespace HoloLensCommander
{
    public class AppCenterWebRequest
    {
        public async Task<string> HttpWebRequest()
        {
            HttpClient httpClient = new HttpClient();

            var request = new HttpRequestMessage();

            request.Headers.Accept.Add(new HttpMediaTypeWithQualityHeaderValue("application/json"));
            request.Headers.Append("X-API-Token", "9b8e356d29f80de93d88fac0252e48f8fbe22e96");

            Uri uri = new Uri("https://api.appcenter.ms/v0.1/apps");
            request.RequestUri = uri;


            HttpResponseMessage httpResponseMessage = new HttpResponseMessage();
            string httpResponseBody = "";

            try
            {
                httpResponseMessage = await httpClient.SendRequestAsync(request);
                httpResponseMessage.EnsureSuccessStatusCode();
                httpResponseBody = await httpResponseMessage.Content.ReadAsStringAsync();
            }
            catch (Exception ex)
            {
                httpResponseBody = "Error: " + ex.HResult.ToString("X") + " Message: " + ex.Message;
            }

            return httpResponseBody;
        }

        /// <summary>
        /// A web request for latest App Center release of the selected app. Release information includes download URL. 
        /// </summary>
        /// <param name="appSecret"></param>
        /// <returns></returns>
        public async Task<string> HttpReleaseRequest(string appSecret)
        {
            HttpClient httpClient = new HttpClient();

            var request = new HttpRequestMessage();

            request.Headers.Accept.Add(new HttpMediaTypeWithQualityHeaderValue("application/json"));
            request.Headers.Append("X-API-Token", "9b8e356d29f80de93d88fac0252e48f8fbe22e96"); //TODO remove hardcoded API Token 

            Uri uri = new Uri("https://api.appcenter.ms/v0.1/sdk/apps/"+ appSecret+"/releases/latest"); //how do I format this?
            request.RequestUri = uri;

            HttpResponseMessage httpResponseMessage = new HttpResponseMessage();
            string httpResponseBody = "";

            try
            {
                httpResponseMessage = await httpClient.SendRequestAsync(request);
                httpResponseMessage.EnsureSuccessStatusCode();
                httpResponseBody = await httpResponseMessage.Content.ReadAsStringAsync();
            }
            catch (Exception ex)
            {
                httpResponseBody = "Error: " + ex.HResult.ToString("X") + " Message: " + ex.Message;
            }

            return httpResponseBody;
        }

        /// <summary>
        /// A web request to download a release of an app.
        /// </summary>
        /// <param name="downloadUrl"></param>
        /// <param name="appName"></param>
        public async void HttpDownloadRequest(string downloadUrl, string appName)
        {
            try
            {
                Uri uri = new Uri(downloadUrl);
                HttpClient httpClient = new HttpClient();

                IInputStream content = await httpClient.GetInputStreamAsync(uri);

                HttpStreamContent httpStream = new HttpStreamContent(content);
                IBuffer buffer = await httpStream.ReadAsBufferAsync();

                StorageFolder cacheFolder = ApplicationData.Current.LocalCacheFolder;
                StorageFile file = await cacheFolder.CreateFileAsync(appName+".appxbundle", CreationCollisionOption.GenerateUniqueName);

                IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.ReadWrite);

                using (var outputStream = stream.GetOutputStreamAt(0))
                {
                    using (var dataWriter = new DataWriter(outputStream))
                    {
                        dataWriter.WriteBuffer(buffer);
                        await dataWriter.StoreAsync();
                        await outputStream.FlushAsync();
                    }
                }
                stream.Dispose();
                httpStream.Dispose();

                //Unzipping the download

                //var archive = await cacheFolder.GetFileAsync(appName + ".zip");
                //ZipFile.ExtractToDirectory(archive.Path, cacheFolder.Path);


                //Background downloader 

                //BackgroundDownloader backgroundDownloader = new BackgroundDownloader();
                ////DownloadOperation downloadOperation = await backgroundDownloader.CreateDownloadAsync(uri, downloadedAppx, inputStream);
                //DownloadOperation downloadOperation = backgroundDownloader.CreateDownload(uri, downloadedAppx);
                ////operation = await Task.Run(() => { return downloadOperation.StartAsync(); });


            }
            catch (Exception)
            {
                throw;
            }
            finally
            {
                await Windows.System.Launcher.LaunchFolderAsync(ApplicationData.Current.LocalCacheFolder);
            }
        }
    }
}