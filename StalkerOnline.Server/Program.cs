using System.Net;
using StalkerOnline.Server.Core;

Console.Title = "Stalker Online Server";
Console.OutputEncoding = System.Text.Encoding.UTF8;

using CancellationTokenSource cancellationTokenSource = new();

Console.CancelKeyPress += (_, eventArgs) =>
{
    eventArgs.Cancel = true;
    cancellationTokenSource.Cancel();
};

GameServer server = new(IPAddress.Any, 7777);

try
{
    await server.StartAsync(cancellationTokenSource.Token);
}
catch (OperationCanceledException)
{
    // Сервер остановлен через CTRL+C
}
catch (Exception ex)
{
    Console.WriteLine($"[FATAL ERROR] {ex.Message}");
}
finally
{
    server.Stop();
}