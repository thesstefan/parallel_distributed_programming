using System.Net;

namespace HttpCrawler.Http;

class Utils
{
    public const int HttpPort = 80;
    
    public static string GetRequestString(IPHostEntry hostEntry, IPEndPoint endPoint)
    {
        return "GET " + endPoint.Address + " HTTP/1.1\r\n" +
               "Host: " + hostEntry.HostName + "\r\n" +
               "Content-Length: 0\r\n\r\n" +
               "Content-Type: text/html";
    }
    
    public static bool CheckHeaderObtained(string response)
    {
        return response.Contains("\r\n\r\n");
    }

    public static string GetResponseBody(string response)
    {
        var responseSplit = response.Split(new[] { "\r\n\r\n" }, StringSplitOptions.RemoveEmptyEntries);
        return responseSplit.Length > 1 ? responseSplit[1] : "";
    }

    public static int GetContentLength(string response)
    {
        var length = 0;
        var lines = response.Split('\r', '\n');

        foreach (var line in lines)
        {
            var headerDetails = line.Split(':');
                
            if (string.Compare(headerDetails[0], "Content-Length", StringComparison.Ordinal) == 0)
            {
                length = int.Parse(headerDetails[1]);
            }
        }

        return length;
    }
}