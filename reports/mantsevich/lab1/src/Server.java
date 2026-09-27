import java.io.*;
import java.net.*;

public class Server {

    public static final int PORT = 8080;

    public static void main(String[] args) {
        try (ServerSocket serverSocket = new ServerSocket(PORT)) {
            System.out.println("Сервер запущен на порту " + PORT);
            
            while (true) {
                Socket clientSocket = serverSocket.accept();
                System.out.println("Подключился клиент: " + clientSocket);

                new Thread(new ClientHandler(clientSocket)).start();
            }
        } catch (IOException e) {
            System.err.println("Ошибка сервера: " + e.getMessage());
        }
    }
}


class ClientHandler implements Runnable {
    private Socket socket;

    public ClientHandler(Socket socket) {
        this.socket = socket;
    }

    @Override
    public void run() {
        try (
            BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            PrintWriter out = new PrintWriter(new BufferedWriter(new OutputStreamWriter(socket.getOutputStream())), true);
        ) {
            out.println("Hi, Ruslan!");

            StringBuilder groupBuffer = new StringBuilder();
            StringBuilder slidingWindow = new StringBuilder();
            int totalChars = 0;
            int ch;

            while ((ch = in.read()) != -1) {
                char c = (char) ch;
                totalChars++;

                slidingWindow.append(c);
                if (slidingWindow.length() > 3) {
                    slidingWindow.deleteCharAt(0);
                }
                if (slidingWindow.toString().equals("~#~")) {
                    out.println("Session terminated. Goodbye!");
                    System.out.println("Обнаружена последовательность ~#~. Завершение сеанса.");
                    break;
                }

                groupBuffer.append(c);

                if (groupBuffer.length() == 48) {
                    int checksum = 0;
                    for (int i = 0; i < groupBuffer.length(); i++) {
                        checksum += (int) groupBuffer.charAt(i);
                    }
                    
                    out.println("Checksum: " + checksum + ", Total chars: " + totalChars);
                    System.out.println("Отправлена группа 48 символов. Checksum=" + checksum + ", Total=" + totalChars);
                    
                    groupBuffer.setLength(0);
                }
            }
        } catch (IOException e) {
            System.out.println("Клиент отключился или ошибка ввода-вывода: " + e.getMessage());
        } finally {
            try {
                socket.close();
            } catch (IOException e) {
            }
        }
    }
}