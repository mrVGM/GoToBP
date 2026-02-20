param(
    [int]$port,
    [string]$url
)

$server = "127.0.0.1"
$message = $url + "|"

$client = [System.Net.Sockets.TcpClient]::new($server, $port)
$stream = $client.GetStream()

$bytes = [System.Text.Encoding]::UTF8.GetBytes($message)
$stream.Write($bytes, 0, $bytes.Length)
$stream.Flush()

# Important: signal end of sending
$client.Client.Shutdown([System.Net.Sockets.SocketShutdown]::Send)

$stream.Close()
$client.Close()
