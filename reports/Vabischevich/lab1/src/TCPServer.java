import java.io.*;
import java.net.*;

public class TCPServer {
    public static final int PORT = 8080;

    public static void main(String[] args) throws IOException {
        ServerSocket serverSocket = new ServerSocket(PORT);
        System.out.println("TCP сервер запущен на порту " + PORT);

        while (true) {
            Socket socket = serverSocket.accept();
            System.out.println("Подключён клиент: " + socket);

            try {
                BufferedReader in = new BufferedReader(
                        new InputStreamReader(socket.getInputStream()));
                PrintWriter out = new PrintWriter(
                        new OutputStreamWriter(socket.getOutputStream()), true);


                out.println("Введите текст. Каждые 10 символов сервер вернёт сумму ASCII-кодов.");

                int sum = 0;     // сумма ASCII-кодов
                int count = 0;   // количество накопленых символов
                String line;


                while ((line = in.readLine()) != null) {
                    for (int i = 0; i < line.length(); i++) {
                        char c = line.charAt(i);

                        sum += c;
                        count++;

                        if (count == 10) {
                            out.println("Checksum = " + sum);
                            System.out.println("Отправлено: " + sum);

                            sum = 0;
                            count = 0;
                        }
                    }
                }
            } catch (IOException e) {
                System.err.println("Ошибка клиента: " + e.getMessage());
            } finally {
                socket.close();
                System.out.println("Клиент отключён.");
            }
        }
    }
}