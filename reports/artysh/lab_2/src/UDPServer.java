import java.io.File;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashSet;
import java.util.Set;

public class UDPServer {
    public static final int PORT = 8080;
    private static final Set<String> disconnectedClients = new HashSet<>();

    public static void main(String[] args) throws IOException {
        DatagramSocket socket = new DatagramSocket(PORT);
        System.out.println("Сервер запущен на порту " + PORT);
        byte[] buffer = new byte[65535];

        while (true) {
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            socket.receive(packet);
            
            String clientKey = packet.getAddress() + ":" + packet.getPort();
            
            if (disconnectedClients.contains(clientKey)) {
                continue;
            }

            String line = new String(packet.getData(), 0, packet.getLength(), StandardCharsets.UTF_8).trim();
            System.out.println("Получено: " + line);

            String response = "";
            boolean shouldDisconnect = false;

            if (line.startsWith("load ")) {
                String filename = line.substring(5).trim();
                if (filename.isEmpty() || filename.contains("/") || filename.contains("\\") || filename.contains("..")) {
                    response = "ERROR: Недопустимое имя файла";
                    shouldDisconnect = true;
                } else {
                    File file = new File(filename);
                    if (file.exists() && file.isFile()) {
                        try {
                            String content = Files.readString(file.toPath(), StandardCharsets.UTF_8);
                            response = content + "\n###END_OF_FILE###";
                        } catch (IOException e) {
                            response = "ERROR: Ошибка чтения файла";
                            shouldDisconnect = true;
                        }
                    } else {
                        response = "ERROR: Файл не найден: " + filename;
                        shouldDisconnect = true;
                    }
                }
            } else if (line.startsWith("create ")) {
                String[] parts = line.split(" ", 3);
                if (parts.length < 3 || parts[2].trim().isEmpty()) {
                    response = "SESSION ENDED";
                    shouldDisconnect = true;
                } else {
                    String filename = parts[1];
                    String text = parts[2];
                    if (filename.isEmpty() || filename.contains("/") || filename.contains("\\") || filename.contains("..")) {
                        response = "SESSION ENDED";
                        shouldDisconnect = true;
                    } else {
                        try {
                            Path filePath = Paths.get(filename);
                            Files.writeString(filePath, text, StandardCharsets.UTF_8);
                            long size = Files.size(filePath);
                            if (size == 0) {
                                response = "SESSION ENDED";
                                shouldDisconnect = true;
                            } else {
                                response = "SIZE: " + size;
                            }
                        } catch (IOException e) {
                            response = "SESSION ENDED";
                            shouldDisconnect = true;
                        }
                    }
                }
            } else {
                response = "UNKNOWN COMMAND";
            }

            if (shouldDisconnect) {
                disconnectedClients.add(clientKey);
            }

            byte[] responseData = response.getBytes(StandardCharsets.UTF_8);
            DatagramPacket responsePacket = new DatagramPacket(responseData, responseData.length, packet.getAddress(), packet.getPort());
            socket.send(responsePacket);
        }
    }
}