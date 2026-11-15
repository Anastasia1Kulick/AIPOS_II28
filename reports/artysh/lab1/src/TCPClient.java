import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;

public class TCPClient {
    private Socket socket;
    private BufferedReader in;
    private PrintWriter out;
    private PrintWriter logWriter;
    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");
    private volatile boolean connected = false;
    private final Object consoleLock = new Object();

    public static void main(String[] args) {
        new TCPClient().run();
    }

    public void run() {
        initLog();
        log("Клиент запущен");

        BufferedReader console = new BufferedReader(
                new InputStreamReader(System.in, StandardCharsets.UTF_8));
        try {
            String line;
            while (true) {
                synchronized (consoleLock) {
                    System.out.print("> ");
                    System.out.flush();
                }
                line = console.readLine();
                if (line == null) break;
                if (line.trim().isEmpty()) continue;

                if (line.startsWith("connect ")) {
                    String[] parts = line.split("\\s+");
                    if (parts.length == 3) {
                        try {
                            int port = Integer.parseInt(parts[2]);
                            connect(parts[1], port);
                        } catch (NumberFormatException e) {
                            System.out.println("Неверный номер порта.");
                        }
                    } else {
                        System.out.println("Использование: connect <адрес> <порт>");
                    }
                    continue;
                }

                if (!connected || socket == null || socket.isClosed()) {
                    System.out.println("Нет соединения. Введите: connect <адрес> <порт>");
                    continue;
                }

                log("ОТПРАВЛЕНО: " + line);
                out.println(line);
            }
        } catch (IOException e) {
            log("Ошибка ввода-вывода: " + e.getMessage());
        } finally {
            disconnect();
        }
    }

    private void initLog() {
        try {
            logWriter = new PrintWriter(new OutputStreamWriter(
                    new FileOutputStream("client.log", true), StandardCharsets.UTF_8), true);
        } catch (IOException e) {
            System.err.println("Не удалось открыть файл протокола: " + e.getMessage());
        }
    }

    private void log(String msg) {
        String time = sdf.format(new Date());
        String line = "[" + time + "] " + msg;
        synchronized (consoleLock) {
            System.out.println(line);
        }
        if (logWriter != null) logWriter.println(line);
    }

    private void connect(String host, int port) {
        if (connected) {
            log("Переподключение: закрываем текущее соединение");
            disconnect();
        }
        try {
            socket = new Socket(host, port);
            in = new BufferedReader(new InputStreamReader(
                    socket.getInputStream(), StandardCharsets.UTF_8));
            out = new PrintWriter(new OutputStreamWriter(
                    socket.getOutputStream(), StandardCharsets.UTF_8), true);
            connected = true;
            log("Соединение установлено с " + host + ":" + port
                    + " в " + sdf.format(new Date()));
            new Thread(this::readLoop, "reader").start();
        } catch (IOException e) {
            log("Ошибка подключения: " + e.getMessage());
        }
    }


    private void readLoop() {
        try {
            String line;
            while ((line = in.readLine()) != null) {
                log("ПОЛУЧЕНО: " + line);
                if (line.equals("SESSION ENDED")) {
                    log("Сервер завершил сеанс. Соединение будет закрыто.");
                    break;
                }
            }
        } catch (IOException e) {
            if (connected) log("Соединение разорвано: " + e.getMessage());
        } finally {
            disconnect();
        }
    }

    private void disconnect() {
        if (!connected) return;
        connected = false;
        log("Отключение в " + sdf.format(new Date()));
        try {
            if (socket != null) socket.close();
        } catch (IOException ignored) {
        }
        socket = null;
        in = null;
        out = null;
    }
}
