import java.io.*;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Scanner;

public class UdpClient {
    private static final String DEFAULT_HOST = "127.0.0.1";
    private static final int DEFAULT_PORT = 666;
    private static final String PROTOCOL_FILE = "udp_client_protocol.txt";

    private static PrintWriter protocolWriter;
    private static DateTimeFormatter dtf = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

    public static void main(String[] args) {
        // создание лог-файла
        try {
            protocolWriter = new PrintWriter(new OutputStreamWriter(new FileOutputStream(PROTOCOL_FILE, true), StandardCharsets.UTF_8), true);
        } catch (IOException e) {
            System.err.println("Не удалось создать файл протокола: " + e.getMessage());
        }

        Scanner scanner = new Scanner(System.in, StandardCharsets.UTF_8);
        
        try (DatagramSocket socket = new DatagramSocket()) {
            
            // подключение
            InetAddress serverAddress = InetAddress.getByName(DEFAULT_HOST);
            socket.connect(serverAddress, DEFAULT_PORT); // привязка UDP-сокета
            logProtocol("Начало соединения с " + DEFAULT_HOST + ":" + DEFAULT_PORT);
            System.out.println("Автоматически подключено к UDP-серверу " + DEFAULT_HOST + ":" + DEFAULT_PORT);

            System.out.println("\nДоступные команды:");
            System.out.println(" - load <имя_файла> (запросить файл у сервера)");
            System.out.println(" - disconnect <адрес> <порт> (отключиться от сервера)");
            System.out.println(" - exit (выход из программы)\n");

            byte[] receiveBuffer = new byte[65535]; // буфер для приема

            while (true) {
                System.out.print("> ");
                String inputLine = scanner.nextLine().trim();

                if ("exit".equalsIgnoreCase(inputLine)) {
                    logProtocol("Окончание соединения (выход из программы).");
                    break;
                }

                // команда disconnect <адрес> <порт>
                if (inputLine.startsWith("disconnect")) {
                    socket.disconnect();
                    logProtocol("Окончание соединения по команде disconnect.");
                    System.out.println("Соединение разорвано. Вы больше не привязаны к серверу.");
                    continue;
                }

                // отправка на сервер
                if (inputLine.startsWith("load ")) {
                    if (!socket.isConnected()) {
                        System.out.println("Ошибка: Соединение разорвано. Перезапустите клиент для подключения.");
                        continue;
                    }

                    // логи
                    String sendTime = LocalDateTime.now().format(dtf);
                    logProtocol("Передана строка: \"" + inputLine + "\" в " + sendTime);

                    // отправляем датаграмму
                    byte[] sendData = inputLine.getBytes(StandardCharsets.UTF_8);
                    DatagramPacket sendPacket = new DatagramPacket(sendData, sendData.length, serverAddress, DEFAULT_PORT);
                    socket.send(sendPacket);

                    // ожидаем ответ
                    DatagramPacket receivePacket = new DatagramPacket(receiveBuffer, receiveBuffer.length);
                    socket.receive(receivePacket);
                    
                    String response = new String(receivePacket.getData(), 0, receivePacket.getLength(), StandardCharsets.UTF_8);
                    
                    // если сервер прислал сообщение об ошибке и разрыве
                    if (response.startsWith("DISCONNECT:")) {
                        System.out.println("Ответ сервера: " + response.substring(11).trim());
                        System.out.println("Сервер принудительно разорвал соединение.");
                        socket.disconnect();
                        logProtocol("Окончание соединения (инициализировано сервером из-за отсутствия файла).");
                    } else {
                        System.out.println("=== Содержимое файла ===");
                        System.out.println(response);
                        System.out.println("========================");
                    }
                } else {
                    System.out.println("Неизвестная команда.");
                }
            }
        } catch (IOException e) {
            System.err.println("Ошибка сетевого взаимодействия: " + e.getMessage());
            logProtocol("Окончание соединения из-за ошибки: " + e.getMessage());
        } finally {
            if (protocolWriter != null) {
                protocolWriter.close();
            }
            scanner.close();
        }
    }

    // запись
    private static void logProtocol(String message) {
        String logEntry = "[" + LocalDateTime.now().format(dtf) + "] " + message;
        if (protocolWriter != null) {
            protocolWriter.println(logEntry);
        }
    }
}
