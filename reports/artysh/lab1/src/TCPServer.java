import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;

public class TCPServer {
    public static final int PORT = 8080;

    public static void main(String[] args) throws IOException {
        ServerSocket serverSocket = new ServerSocket(PORT);
        System.out.println("Сервер запущен на порту " + PORT);

        while (true) {
            Socket clientSocket = serverSocket.accept();
            System.out.println("Подключился клиент: " + clientSocket);
            new Thread(new ClientHandler(clientSocket)).start();
        }
    }
}

class ClientHandler implements Runnable {
    private final Socket socket;

    public ClientHandler(Socket socket) {
        this.socket = socket;
    }

    @Override
    public void run() {
        try (BufferedReader in = new BufferedReader(
                new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
             PrintWriter out = new PrintWriter(
                new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8), true)) {

            

            String line;
            while ((line = in.readLine()) != null) {
                System.out.println("Получено: " + line);

                
                if (line.startsWith("load ")) {
                    String filename = line.substring(5).trim();

                    
                    if (filename.isEmpty()
                            || filename.contains("/")
                            || filename.contains("\\")
                            || filename.contains("..")) {
                        out.println("ERROR: Недопустимое имя файла");
                        break;
                    }

                    File file = new File(filename);
                    if (file.exists() && file.isFile()) {
                        try (BufferedReader fileReader = new BufferedReader(
                                new InputStreamReader(
                                        new FileInputStream(file), StandardCharsets.UTF_8))) {
                            String fileLine;
                            while ((fileLine = fileReader.readLine()) != null) {
                                out.println(fileLine);
                            }
                        }
                        out.println("###END_OF_FILE###");
                    } else {
                        out.println("ERROR: Файл не найден: " + filename);
                        break; 
                    }
                }
                
                else if (line.startsWith("create ")) {
                    String[] parts = line.split(" ", 3);

                    if (parts.length < 3 || parts[2].trim().isEmpty()) {
                        out.println("SESSION ENDED");
                        break;
                    }

                    String filename = parts[1];
                    String text = parts[2];

                    if (filename.isEmpty()
                            || filename.contains("/")
                            || filename.contains("\\")
                            || filename.contains("..")) {
                        out.println("SESSION ENDED");
                        break;
                    }

                    try {
                        Path filePath = Paths.get(filename);
                        Files.writeString(filePath, text, StandardCharsets.UTF_8);
                        long size = Files.size(filePath);


                        if (size == 0) {
                            out.println("SESSION ENDED");
                            break;
                        }
                        out.println("SIZE: " + size);
                    } catch (IOException e) {
                        System.err.println("Ошибка создания файла: " + e.getMessage());
                        out.println("SESSION ENDED");
                        break;
                    }
                }
                
                else {
                    out.println("UNKNOWN COMMAND");
                }
            }
        } catch (IOException e) {
            System.err.println("Ошибка клиента: " + e.getMessage());
        } finally {
            try {
                socket.close();
            } catch (IOException ignored) {
            }
        }
    }
}
