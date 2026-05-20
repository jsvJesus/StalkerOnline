using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using StalkerOnline.Server.Services;

namespace StalkerOnline.Server.Core;

public sealed class GameServer
{
    private readonly IPAddress _ipAddress;
    private readonly int _port;

    private readonly AccountService _accountService = new();

    private readonly ConcurrentDictionary<int, ClientSession> _sessions = new();

    private TcpListener? _listener;
    private int _nextSessionId;

    public GameServer(IPAddress ipAddress, int port)
    {
        _ipAddress = ipAddress;
        _port = port;
    }

    public async Task StartAsync(CancellationToken cancellationToken = default)
    {
        _listener = new TcpListener(_ipAddress, _port);
        _listener.Start();

        Console.WriteLine("===================================");
        Console.WriteLine(" Stalker Online Server started");
        Console.WriteLine($" Address: {_ipAddress}");
        Console.WriteLine($" Port: {_port}");
        Console.WriteLine("===================================");

        while (!cancellationToken.IsCancellationRequested)
        {
            TcpClient tcpClient = await _listener.AcceptTcpClientAsync(cancellationToken);

            int sessionId = Interlocked.Increment(ref _nextSessionId);

            ClientSession session = new(
                sessionId,
                tcpClient,
                _accountService,
                RemoveSession);

            if (!_sessions.TryAdd(sessionId, session))
            {
                Console.WriteLine($"[SESSION ERROR] Failed to add session: {sessionId}");
                tcpClient.Close();
                continue;
            }

            Console.WriteLine($"[SESSION ADDED] SessionId={sessionId}, Online={_sessions.Count}");

            _ = Task.Run(session.RunAsync, cancellationToken);
        }
    }

    private void RemoveSession(ClientSession session)
    {
        if (_sessions.TryRemove(session.SessionId, out _))
        {
            Console.WriteLine($"[SESSION REMOVED] SessionId={session.SessionId}, Online={_sessions.Count}");
        }
    }

    public void Stop()
    {
        Console.WriteLine("[SERVER STOPPING]");

        foreach (ClientSession session in _sessions.Values)
        {
            session.Close();
        }

        _sessions.Clear();

        _listener?.Stop();

        Console.WriteLine("[SERVER STOPPED]");
    }
}