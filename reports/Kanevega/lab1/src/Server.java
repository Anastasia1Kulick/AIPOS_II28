import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public class Server {
    public static void main(String[] args) {
        int port = 12345; // порт
        
        System.out.println("Запуск сервера...");
        try (ServerSocket serverSocket = new ServerSocket(port)) {
            System.out.println("Сервер ожидает подключения на порту " + port + "...");
            
            try (Socket socket = serverSocket.accept()) {
                System.out.println("Клиент успешно подключился!");
                
                BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
                PrintWriter out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8), true);
                
                String str;
                while ((str = in.readLine()) != null) {
                    str = str.trim();
                    
                    if ("END".equalsIgnoreCase(str)) {
                        out.println("Сеанс завершен по запросу клиента.");
                        break;
                    }
                    
                    // начинается ли строка с команды "create "
                    if (str.startsWith("create ")) {
                        // формат: create <имя файла> <текст>
                        // Разделяем строку на 3 части: команда, имя файла, текст
                        String[] parts = str.split(" ", 3);
                        
                        if (parts.length < 3) {
                            out.println("ERROR: Неверный формат команды. Используйте: create <имя_файла> <текст>");
                            continue;
                        }
                        
                        String fileName = parts[1];
                        String fileContent = parts[2];
                        
                        File file = new File(fileName);
                        boolean created = false;
                        
                        try {
                            // Создаем файл и записываем в него текст
                            try (BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8))) {
                                writer.write(fileContent);
                                created = true;
                            }
                        } catch (IOException e) {
                            created = false;
                        }
                        
                        // Проверяем условия: файл должен создаться, существовать и иметь размер > 0
                        if (!created || !file.exists() || file.length() == 0) {
                            out.println("END");
                            System.out.println("Ошибка создания файла или его размер равен нулю. Разрыв соединения.");
                            break;
                        }
                        
                        long fileSize = file.length();
                        System.out.println("Файл '" + fileName + "' успешно создан. Размер: " + fileSize + " байт.");
                        // Отсылаем размер файла
                        out.println("FILE_SIZE: " + fileSize);
                        
                    } else {
                        // Обработка прочих команд
                        System.out.println("Получено: " + str);
                        out.println("Echo: " + str);
                    }
                }
            }
        } catch (IOException e) {
            System.err.println("Ошибка в работе сервера: " + e.getMessage());
            e.printStackTrace();
        }
        System.out.println("Сервер остановлен.");
    }
}