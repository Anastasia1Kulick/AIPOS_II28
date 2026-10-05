import java.io.*;
import java.net.*;

public class TCPServer {
    
    public static final int PORT = 8080;

    public static void main(String[] args) throws IOException {
        
        try (ServerSocket serverSocket = new ServerSocket(PORT)) {
            System.out.println("Сервер запущен на порту " + PORT);

            
            while (true) {
                
                Socket clientSocket = serverSocket.accept();
                System.out.println("Подключился клиент: " + clientSocket);

                
                new Thread(new ClientHandler(clientSocket)).start();
            }
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
        
        try (BufferedReader in = new BufferedReader(
                new InputStreamReader(socket.getInputStream()));
             PrintWriter out = new PrintWriter(
                new OutputStreamWriter(socket.getOutputStream()), true)) {

            String line;
            
            while ((line = in.readLine()) != null) {
                System.out.println("Получено: " + line);

                
                if (line.startsWith("load ")) {
                    String filename = line.substring(5).trim(); 
                    File file = new File(filename);

                    if (file.exists() && file.isFile()) {
                        
                        try (BufferedReader fileReader = new BufferedReader(
                                new FileReader(file))) {
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
                } else {
                    out.println("UNKNOWN COMMAND");
                }
            }
        } catch (IOException e) {
            System.err.println("Ошибка клиента: " + e.getMessage());
        } finally {
            try {
                socket.close();
            } catch (IOException e) {
                
            }
        }
    }
}