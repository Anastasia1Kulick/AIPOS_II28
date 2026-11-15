import java.io.BufferedReader;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;

public class UDPClient {
    private DatagramSocket socket;
    private InetAddress serverAddress;
    private int serverPort;
    private PrintWriter logWriter;
    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");
    private volatile boolean connected = false;
    private final Object consoleLock = new Object();

    public static void main(String[] args) {
        new UDPClient().run();
    }

    public void run() {
        initLog();
        log("Клиент запущен");
        BufferedReader console = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
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
                            connect(parts[1], Integer.parseInt(parts[2]));
                        } catch (NumberFormatException e) {
                            System.out.println("Неверный номер порта.");
                        }
                    } else {
                        System.out.println("Использование: connect <адрес> <порт>");
                    }
                    continue;
                }

                if (line.startsWith("disconnect")) {
                    disconnect();
                    continue;
                }

                if (!connected || socket == null || socket.isClosed()) {
                    System.out.println("Нет соединения. Введите: connect <адрес> <порт>");
                    continue;
                }

                log("ОТПРАВЛЕНО: " + line);
                sendDatagram(line);
            }
        } catch (IOException e) {
            log("Ошибка ввода-вывода: " + e.getMessage());
        } finally {
            disconnect();
        }
    }

    private void initLog() {
        try {
            logWriter = new PrintWriter(new OutputStreamWriter(new FileOutputStream("client.log", true), StandardCharsets.UTF_8), true);
        } catch (IOException e) {
            System.err.println("Не удалось открыть файл протокола: " + e.getMessage());
        }
    }

    private void log(String msg) {
        String time = sdf.format(new Date());
        String logLine = "[" + time + "] " + msg;
    
        synchronized (consoleLock) {
            System.out.println(msg);
        }
        

        if (logWriter != null) {
            logWriter.println(logLine);
        }
    }

    private void connect(String host, int port) {
        if (connected) {
            log("Переподключение: закрываем текущее соединение");
            disconnect();
        }
        try {
            serverAddress = InetAddress.getByName(host);
            serverPort = port;
            socket = new DatagramSocket();
            connected = true;
            log("Установлено соединение с " + host + ":" + port);
            new Thread(this::readLoop, "reader").start();
        } catch (UnknownHostException | SocketException e) {
            log("Ошибка подключения: " + e.getMessage());
        }
    }

    private void sendDatagram(String message) {
        try {
            byte[] data = message.getBytes(StandardCharsets.UTF_8);
            DatagramPacket packet = new DatagramPacket(data, data.length, serverAddress, serverPort);
            socket.send(packet);
        } catch (IOException e) {
            log("Ошибка отправки: " + e.getMessage());
        }
    }

    private void readLoop() {
        byte[] buffer = new byte[65535];
        try {
            while (connected && socket != null && !socket.isClosed()) {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                socket.receive(packet);
                String line = new String(packet.getData(), 0, packet.getLength(), StandardCharsets.UTF_8).trim();
                log("ПОЛУЧЕНО: " + line);
                if (line.equals("SESSION ENDED") || line.startsWith("ERROR:")) {
                    log("Сервер завершил сеанс. Соединение будет закрыто.");
                    break;
                }
            }
        } catch (SocketException e) {
        } catch (IOException e) {
            if (connected) {
                log("Соединение разорвано: " + e.getMessage());
            }
        } finally {
            disconnect();
        }
    }

    private void disconnect() {
        if (!connected) return;
        connected = false;
        log("Сеанс завершен: отключение от " + serverAddress + ":" + serverPort);
        if (socket != null && !socket.isClosed()) {
            try {
                sendDatagram("disconnect");
            } catch (Exception ignored) {
            }
            socket.close();
        }
        socket = null;
        serverAddress = null;
        serverPort = 0;
    }
}