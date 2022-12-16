namespace HttpCrawler.Connection;

public static class ConnectionLogger
{
    public static void LogConnect(ConnectionState state)
    {
        Console.WriteLine("{0}: Socket connected to {1}.", 
            state.Id, state.HostEntry.HostName);
    }

    public static void LogSend(ConnectionState state, int bytesSent)
    {
        Console.WriteLine("{0}: Sent {1} bytes to server.", 
            state.Id, bytesSent);
    }

    public static void LogReceive(ConnectionState state)
    {
        Console.WriteLine("{0}: Received {1} bytes from server.",
            state.Id, state.ResponseContent.Length);
    }
}