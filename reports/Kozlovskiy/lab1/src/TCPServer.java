import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;

public class TCPServer {

    public static final int PORT = 8080;
    public static final int CHUNK_SIZE = 10;

    public static void main(String[] args) throws IOException {
        int port = (args.length > 0) ? Integer.parseInt(args[0]) : PORT;

        ServerSocket serverSocket = new ServerSocket(port);
        System.out.println("TCP server started: " + serverSocket);
        try {
            while (true) {
                // блокируется до подключения клиента
                Socket socket = serverSocket.accept();
                System.out.println("Connection accepted: " + socket);
                // каждый клиент обслуживается в своём потоке
                new ClientHandler(socket).start();
            }
        } finally {
            serverSocket.close();
        }
    }
}

class ClientHandler extends Thread {

    // служебные коды telnet, нужны только чтобы пропускать их в потоке
    private static final int IAC = 255;
    private static final int DONT = 254;
    private static final int WILL = 251;
    private static final int SB = 250;
    private static final int SE = 240;

    private final Socket socket;
    private final InputStream in;
    private final OutputStream out;
    private final String clientName;

    ClientHandler(Socket socket) throws IOException {
        this.socket = socket;
        this.in = new BufferedInputStream(socket.getInputStream());
        this.out = socket.getOutputStream();
        this.clientName = socket.getInetAddress().getHostAddress() + ":" + socket.getPort();
        setDaemon(true);
    }

    @Override
    public void run() {
        try {
            StringBuilder chunk = new StringBuilder();
            int b;
            // читаем поток до закрытия соединения клиентом (-1)
            while ((b = readDataByte()) != -1) {
                if (b == '\r' || b == '\n' || b == 0) {
                    continue;
                }
                chunk.append((char) b);

                if (chunk.length() == TCPServer.CHUNK_SIZE) {
                    String answer = "Checksum of [" + printable(chunk) + "] = " + checksum(chunk);
                    send(answer);
                    System.out.println("[" + clientName + "] " + answer);
                    chunk.setLength(0);
                }
            }
            System.out.println("[" + clientName + "] client disconnected"
                    + " (incomplete chunk discarded: " + chunk.length() + " chars)");
        } catch (IOException e) {
            System.out.println("[" + clientName + "] connection error: " + e.getMessage());
        } finally {
            try {
                socket.close();
            } catch (IOException ignored) {
            }
        }
    }

    // сумма ASCII-кодов символов
    private static int checksum(CharSequence chars) {
        int sum = 0;
        for (int i = 0; i < chars.length(); i++) {
            sum += chars.charAt(i);
        }
        return sum;
    }

    // непечатные и не-ASCII символы заменяются на '?' только для вывода
    private static String printable(CharSequence chars) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < chars.length(); i++) {
            char c = chars.charAt(i);
            sb.append((c >= 32 && c < 127) ? c : '?');
        }
        return sb.toString();
    }

    private void send(String text) throws IOException {
        out.write((text + "\r\n").getBytes(StandardCharsets.US_ASCII));
        out.flush();
    }

    // читает байт данных, пропуская управляющие последовательности telnet;
    // -1 - клиент закрыл соединение
    private int readDataByte() throws IOException {
        while (true) {
            int b = in.read();
            if (b != IAC) {
                return b;
            }
            int cmd = in.read();
            if (cmd == -1) {
                return -1;
            }
            if (cmd == IAC) {
                return IAC; // IAC IAC - обычный байт 255
            }
            if (cmd >= WILL && cmd <= DONT) {
                in.read(); // код опции
            } else if (cmd == SB) {
                // пропускаем всё до IAC SE
                int prev = 0;
                int x;
                while ((x = in.read()) != -1) {
                    if (prev == IAC && x == SE) {
                        break;
                    }
                    prev = x;
                }
            }
        }
    }
}
