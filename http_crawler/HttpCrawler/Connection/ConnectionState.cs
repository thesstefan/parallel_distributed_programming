using System.Net;
using System.Net.Sockets;
using System.Text;
using HttpCrawler.Http;

namespace HttpCrawler.Connection;

public class ConnectionState
{
    public ConnectionState(string url, int id)
    {
        var hostEntry = Dns.GetHostEntry(new UriBuilder(url).Host);
        var ipAddress = hostEntry.AddressList[0];

        Id = id;
        HostEntry = hostEntry;
        Socket = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
        RemoteEndpoint = new IPEndPoint(ipAddress, Utils.HttpPort);
    }

    public readonly Socket Socket;
    public readonly IPEndPoint RemoteEndpoint;
    public readonly IPHostEntry HostEntry;
    public readonly int Id;
    
    public const int ReceiveBufferSize = 1024;
    public readonly byte[] ReceiveBuffer = new byte[ReceiveBufferSize];
    public readonly StringBuilder ResponseContent = new();
    public readonly ManualResetEvent ReceiveDone = new(false);
}