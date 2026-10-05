import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;


public class TcpServer {
    public static final int PORT = 8080;   

    public static void main(String[] args) throws IOException {
        ServerSocket server = new ServerSocket(PORT);
        System.out.println("Server started: " + server);
        try {
            while (true) {
                Socket socket = server.accept();      // ожидание клиента (блокирующий вызов)
                System.out.println("Connection accepted: " + socket);
                try {
                    new ClientHandler(socket).start(); // каждый клиент обслуживается в своём потоке
                } catch (Exception e) {
                    socket.close();
                }
            }
        } finally {
            server.close();
        }
    }
}

/** Поток обслуживания одного клиента. */
class ClientHandler extends Thread {
    private static final int CHAIN_LEN = 10;   // длина цепочки символов
    private final Socket socket;

    ClientHandler(Socket socket) {
        this.socket = socket;
    }

    @Override
    public void run() {
        try {
            // Reader читает символы, PrintWriter с автосбросом буфера (true) отправляет строки
            Reader in = new BufferedReader(
                    new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
            PrintWriter out = new PrintWriter(new BufferedWriter(
                    new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8)), true);

            out.println("Hello, Student!");        // приветствие (проверка через telnet)

            int count = 0;   // сколько символов набрано в текущей цепочке
            int sum = 0;     // контрольная сумма текущей цепочки
            int ch;
            while ((ch = in.read()) != -1) {
                // символы конца строки не входят в цепочку
                if (ch == '\r' || ch == '\n') continue;
                sum += ch;
                count++;
                if (count == CHAIN_LEN) {          // цепочка из 10 символов принята
                    System.out.println(socket.getPort() + ": checksum = " + sum);
                    out.println("Checksum: " + sum);
                    count = 0;
                    sum = 0;
                }
            }
        } catch (IOException e) {
            System.err.println("IO Exception: " + e.getMessage());
        } finally {
            try {
                System.out.println("closing " + socket);
                socket.close();                    // всегда освобождаем сокет
            } catch (IOException e) {
                System.err.println("Socket not closed");
            }
        }
    }
}