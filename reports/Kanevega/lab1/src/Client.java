import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public class Client {
    public static void main(String[] args) {
        String host = "127.0.0.1";
        int port = 12345;

        try (Socket socket = new Socket(host, port);
             BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
             PrintWriter out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8), true);
             BufferedReader consoleInput = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8))) {

            System.out.println("Подключено к серверу. Введите команду (например: create test.txt Привет,мир!):");

            // тестовый файл
            String command = "create test.txt Привет, мир! Это тестовый файл.";
            System.out.println("Отправка: " + command);
            out.println(command);

            // ответ от сервера
            String response = in.readLine();
            System.out.println("Ответ сервера: " + response);

            // конец
            out.println("END");
            System.out.println("Ответ на END: " + in.readLine());

        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}