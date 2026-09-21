import java.io.*;
import java.net.*;

public class TCPServer {
    public static final int PORT = 8080;
    // Количество накапливаемых символов перед отправкой контрольной суммы
    private static final int BLOCK_SIZE = 10;

    public static void main(String[] args) throws IOException {
        ServerSocket serverSocket = new ServerSocket(PORT);
        System.out.println("TCP сервер запущен на порту " + PORT);

        try {
            while (true) {
                // Ожидание подключения нового клиента
                Socket socket = serverSocket.accept();
                System.out.println("Подключён клиент: " + socket);

                // обслуживание клиента в отдельном потоке,
                new Thread(new ClientHandler(socket)).start();
            }
        } finally {
            serverSocket.close();
        }
    }
}

class ClientHandler implements Runnable {
    private final Socket socket;

    ClientHandler(Socket socket) {
        this.socket = socket;
    }

    @Override
    public void run() {
        try (
                Socket s = socket;
                BufferedReader in = new BufferedReader(
                        new InputStreamReader(s.getInputStream(), "US-ASCII"));
                PrintWriter out = new PrintWriter(
                        new OutputStreamWriter(s.getOutputStream(), "US-ASCII"), true)
        ) {
            out.println("Hello, Student!");
            out.println("Введите текст. Каждые " + TCPServer.BLOCK_SIZE
                    + " символов сервер вернёт сумму ASCII-кодов.");

            int sum = 0;     // сумма ASCII-кодов
            int count = 0;   // сколько символов уже накоплено
            String line;

            while ((line = in.readLine()) != null) {
                for (int i = 0; i < line.length(); i++) {
                    char c = line.charAt(i);

                    // Явно проверяем, что символ входит в ASCII (0..127).
                    // Иначе кириллица и другие Unicode-символы дадут
                    // неверный результат, а протокол обещает ASCII.
                    if (c > 127) {
                        out.println("Ошибка: символ вне ASCII: '" + c + "'");
                        System.out.println("Клиент " + s + " прислал не-ASCII символ");
                        return;
                    }

                    sum += c;   // для ASCII c == код символа
                    count++;

                    // Набрали блок из 10 символов — отправляем сумму
                    if (count == TCPServer.BLOCK_SIZE) {
                        out.println("Checksum = " + sum);
                        System.out.println("Клиенту " + s + " отправлена сумма: " + sum);

                        sum = 0;
                        count = 0;
                    }
                }
            }

            // Если остались символы меньше блока — сообщаем остаток
            if (count > 0) {
                out.println("Остаток " + count + " символов, Checksum = " + sum);
            }

            System.out.println("Клиент отключён: " + socket);

        } catch (IOException e) {
            System.err.println("Ошибка клиента: " + e.getMessage());
        }
    }
}