import java.io.File;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public class UdpServer {
    public static void main(String[] args) {
        int port = 666; // Порт

        System.out.println("Запуск UDP-сервера на порту " + port + "...");

        try (DatagramSocket serverSocket = new DatagramSocket(port)) {
            byte[] receiveBuffer = new byte[1024];

            while (true) {
                DatagramPacket receivePacket = new DatagramPacket(receiveBuffer, receiveBuffer.length);
                serverSocket.receive(receivePacket); // Ожидание сообщения

                String clientMessage = new String(receivePacket.getData(), 0, receivePacket.getLength(), StandardCharsets.UTF_8).trim();
                InetAddress clientAddress = receivePacket.getAddress();
                int clientPort = receivePacket.getPort();

                System.out.println("Получен запрос от " + clientAddress + ":" + clientPort + " -> " + clientMessage);

                // команда load <имя файла>
                if (clientMessage.startsWith("load ")) {
                    String fileName = clientMessage.substring(5).trim();
                    File file = new File(fileName);

                    if (file.exists() && !file.isDirectory()) {
                        // Файл найден читаем содержимое и отправляем
                        byte[] fileBytes = Files.readAllBytes(file.toPath());
                        String fileContent = new String(fileBytes, StandardCharsets.UTF_8);
                        
                        sendMessage(serverSocket, fileContent, clientAddress, clientPort);
                        System.out.println("Файл '" + fileName + "' успешно отправлен.");
                    } else {
                        // Файл отсутствует "разрываем соединение"
                        String errorMessage = "DISCONNECT: Запрашиваемый файл '" + fileName + "' отсутствует на сервере.";
                        sendMessage(serverSocket, errorMessage, clientAddress, clientPort);
                        System.out.println("Ошибка: файл не найден. Отправлен сигнал разрыва соединения.");
                    }
                } else {
                    // Обработка неизвестной команды
                    sendMessage(serverSocket, "Неизвестная команда. Используйте: load <имя файла>", clientAddress, clientPort);
                }
            }
        } catch (IOException e) {
            System.err.println("Ошибка сервера: " + e.getMessage());
        }
    }

    // Вспомогательный метод для отправки UDP-пакета
    private static void sendMessage(DatagramSocket socket, String message, InetAddress address, int port) throws IOException {
        byte[] sendData = message.getBytes(StandardCharsets.UTF_8);
        DatagramPacket sendPacket = new DatagramPacket(sendData, sendData.length, address, port);
        socket.send(sendPacket);
    }
}
