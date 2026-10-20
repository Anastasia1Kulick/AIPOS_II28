import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Scanner;

public class Client {
    private static final String DEFAULT_HOST = "127.0.0.1";
    private static final int DEFAULT_PORT = 12345;
    private static final String PROTOCOL_FILE = "client_protocol.txt";
    
    private static PrintWriter protocolWriter;
    private static DateTimeFormatter dtf = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

    public static void main(String[] args) {
        // файл
        try {
            protocolWriter = new PrintWriter(new OutputStreamWriter(new FileOutputStream(PROTOCOL_FILE, true), StandardCharsets.UTF_8), true);
        } catch (IOException e) {
            System.err.println("Не удалось создать файл протокола: " + e.getMessage());
        }

        Scanner scanner = new Scanner(System.in, StandardCharsets.UTF_8);
        Socket socket = null;
        BufferedReader in = null;
        PrintWriter out = null;

        // Автоматическое подключение к серверу
        try {
            logProtocol("Начало соединения с " + DEFAULT_HOST + ":" + DEFAULT_PORT);
            socket = new Socket(DEFAULT_HOST, DEFAULT_PORT);
            in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
            out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8), true);
            System.out.println("Успешно подключено к серверу по умолчанию: " + DEFAULT_HOST + ":" + DEFAULT_PORT);
        } catch (IOException e) {
            System.out.println("Ошибка автоподключения: " + e.getMessage() + ".");
        }

        System.out.println("\nДоступные команды:");
        System.out.println(" - create <имя_файла> <текст> (для отправки нажмите Home или Enter)");
        System.out.println(" - disconnect <адрес> <порт> (разрыв соединения)");
        System.out.println(" - exit (выход из программы)\n");

        while (true) {
            System.out.print("> ");
            String inputLine = scanner.nextLine().trim();

            if ("exit".equalsIgnoreCase(inputLine)) {
                break;
            }

            // разрыв соединения при помощи команды: disconnect <адрес> <порт>
            if (inputLine.startsWith("disconnect")) {
                try {
                    if (socket != null && !socket.isClosed()) {
                        socket.close();
                        logProtocol("Окончание соединения по команде disconnect");
                        System.out.println("Соединение разорвано.");
                    } else {
                        System.out.println("Соединение уже закрыто.");
                    }
                } catch (IOException e) {
                    System.err.println("Ошибка при закрытии соединения: " + e.getMessage());
                }
                continue;
            }

            // ввод символов по нажатию Home / Enter)
            if (inputLine.startsWith("create ")) {
                if (socket == null || socket.isClosed()) {
                    System.out.println("Ошибка: Нет активного соединения с сервером!");
                    continue;
                }

                try {
                    // строка и время передачи (запись в файл)
                    String sendTime = LocalDateTime.now().format(dtf);
                    logProtocol("Передана строка: \"" + inputLine + "\" в " + sendTime);

                    // отправляем строку серверу
                    out.println(inputLine);

                    // получаем ответ от сервера
                    String response = in.readLine();
                    if (response != null) {
                        System.out.println("Ответ сервера: " + response);
                        if ("END".equals(response)) {
                            System.out.println("Сервер инициализировал завершение сеанса.");
                            socket.close();
                            logProtocol("Окончание соединения (получено END от сервера)");
                            break;
                        }
                    }
                } catch (IOException e) {
                    System.err.println("Ошибка связи с сервером: " + e.getMessage());
                    logProtocol("Окончание соединения из-за ошибки: " + e.getMessage());
                }
            } else {
                System.out.println("Неизвестная команда. Используйте: create <имя_файла> <текст>");
            }
        }

        // закрываем
        if (socket != null && !socket.isClosed()) {
             try {
                 socket.close();
             } catch (IOException e) {
                 System.err.println("Ошибка при закрытии соединения: " + e.getMessage());
             }
        }
        if (protocolWriter != null) {
            protocolWriter.close();
        }
        scanner.close();
        System.out.println("Клиент завершил работу.");
    }

    private static void logProtocol(String message) {
        String logEntry = "[" + LocalDateTime.now().format(dtf) + "] " + message;
        if (protocolWriter != null) {
            protocolWriter.println(logEntry);
        }
        System.out.println("[ПРОТОКОЛ] " + logEntry);
    }
}