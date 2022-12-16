using System.Net.Sockets;
using System.Text;
using HttpCrawler.Connection;
using HttpCrawler.Http;

namespace HttpCrawler.Crawler;

public class CallbackCrawler : IHttpCrawler
{
    private static int _doneCount;
    public void Run(IList<string> urls)
    {
        _doneCount = urls.Count;
        foreach (var it in urls.Select((url, id) => new { Url = url, Id = id }) )
        {
            var connectionState = new ConnectionState(it.Url, it.Id);
            connectionState.Socket.BeginConnect(connectionState.RemoteEndpoint, OnConnect, connectionState);
        }

        do
        {
            Thread.Sleep(100);
        } while (_doneCount > 0);
    }

    private static void OnConnect(IAsyncResult result)
    {
        var connectionState = (ConnectionState) result.AsyncState!;
        connectionState.Socket.EndConnect(result);
        ConnectionLogger.LogConnect(connectionState);

        var byteRequestData = Encoding.ASCII.GetBytes(Utils.GetRequestString(
            connectionState.HostEntry, connectionState.RemoteEndpoint));
        
        connectionState.Socket.BeginSend(
            byteRequestData, 0, byteRequestData.Length, 0, OnSend, connectionState);
    }

    private static void OnSend(IAsyncResult result)
    {
        var connectionState = (ConnectionState) result.AsyncState!;
        connectionState.Socket.EndSend(result);

        var bytesSent = connectionState.Socket.EndSend(result);
        ConnectionLogger.LogSend(connectionState, bytesSent);
        
        connectionState.Socket.BeginReceive(
            connectionState.ReceiveBuffer, 0,
            ConnectionState.ReceiveBufferSize, 0, OnReceive, connectionState);
    }

    private static void OnReceive(IAsyncResult result)
    {
        var connectionState = (ConnectionState) result.AsyncState!;
        var bytesReceived = connectionState.Socket.EndReceive(result);

        try
        {
            connectionState.ResponseContent.Append(
                Encoding.ASCII.GetString(connectionState.ReceiveBuffer, 0, bytesReceived));
            var responseContent = connectionState.ResponseContent.ToString();

            if (!Utils.CheckHeaderObtained(responseContent))
            {
                connectionState.Socket.BeginReceive(
                    connectionState.ReceiveBuffer, 0,
                    ConnectionState.ReceiveBufferSize, 0, OnReceive, connectionState);
            }
            else
            {
                var responseBody = Utils.GetResponseBody(responseContent);
                var headerLength = Utils.GetContentLength(responseContent);

                if (responseBody.Length < headerLength)
                {
                    connectionState.Socket.BeginReceive(
                        connectionState.ReceiveBuffer, 0,
                        ConnectionState.ReceiveBufferSize, 0, OnReceive, connectionState);
                }
                else
                {
                    ConnectionLogger.LogReceive(connectionState);

                    connectionState.Socket.Shutdown(SocketShutdown.Both);
                    connectionState.Socket.Close();

                    _doneCount--;
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.ToString());
        }
    }
}