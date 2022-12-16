using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using HttpCrawler.Connection;
using HttpCrawler.Http;

namespace HttpCrawler.Crawler;

public class TaskHttpCrawler : IHttpCrawler
{
    public void Run(IList<string> urls)
    {
        Task.WaitAll(urls.Select((url, id) => new { Url = url, Id = id }).Select(
            it => Task.Factory.StartNew(StartClient, new Tuple<string, int>(it.Url, it.Id))).ToArray());
    }

    private static void StartClient(object? clientData)
    {
        var (url, id) = (Tuple<string, int>)clientData!;
        var connectionState = new ConnectionState(url, id);

        Connect(connectionState).Wait();
        Send(connectionState,
            Http.Utils.GetRequestString(connectionState.HostEntry, connectionState.RemoteEndpoint)).Wait();
        Receive(connectionState).Wait();
        
        connectionState.Socket.Shutdown(SocketShutdown.Both);
        connectionState.Socket.Close();
    }

    private static Task Connect(ConnectionState state)
    {
        var taskCompletion = new TaskCompletionSource<bool>();

        state.Socket.BeginConnect(state.RemoteEndpoint, result =>
        {
            state.Socket.EndConnect(result);
            ConnectionLogger.LogConnect(state);
            taskCompletion.TrySetResult(true);
        }, state);

        return taskCompletion.Task;
    }

    private static Task Send(ConnectionState state, string data)
    {
        var taskCompletion = new TaskCompletionSource<int>();
        var byteData = Encoding.ASCII.GetBytes(data);

        state.Socket.BeginSend(byteData, 0, byteData.Length, 0, (IAsyncResult result) =>
        {
            var bytesSent = state.Socket.EndSend(result);
            ConnectionLogger.LogSend(state, bytesSent);
            taskCompletion.TrySetResult(bytesSent);
        }, state);

        return taskCompletion.Task;
    }

    private static Task Receive(ConnectionState state)
    {
        state.Socket.BeginReceive(
            state.ReceiveBuffer, 0,
            ConnectionState.ReceiveBufferSize, 0, OnReceive, state);

        return Task.FromResult(state.ReceiveDone.WaitOne());
    }

    private static void OnReceive(IAsyncResult result)
    {
        var connectionState = (ConnectionState)result.AsyncState!;
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
                    connectionState.ReceiveDone.Set();
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.ToString());
        }
    }
}