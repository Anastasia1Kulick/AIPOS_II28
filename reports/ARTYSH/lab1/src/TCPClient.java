import java.io.*;
import java.net.*;
import java.text.SimpleDateFormat;
import java.util.Date;

public class TCPClient {
    private Socket socket;
    private BufferedReader in;
    private PrintWriter out;
    private PrintWriter logWriter;
    private SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");
    private volatile boolean connected = false;

    public static void main(String[] args) {
        new TCPClient().run();
    }

    public void run() {
        initLog();
        log("Клиент запущен");

        BufferedReader console = new BufferedReader(new InputStreamReader(System.in));
        try {
            String line;
            while (true) {
                
                System.out.print("> ");
                line = console.readLine();
                if (line == null) break; 

                if (line.trim().isEmpty()) continue;

                
                if (line.startsWith("connect ")) {
                    String[] parts = line.split("\\s+");
                    if (parts.length == 3) {
                        String host = parts[1];
                        int port;
                        try {
                            port = Integer.parseInt(parts[2]);
                        } catch (NumberFormatException e) {
                            System.out.println("Неверный номер порта.");
                            continue;
                        }
                        connect(host, port);
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
            logWriter = new PrintWriter(new FileWriter("client.log", true), true);
        } catch (IOException e) {
            System.err.println("Не удалось открыть файл протокола: " + e.getMessage());
        }
    }

    
    private void log(String msg) {
        String time = sdf.format(new Date());
        String line = "[" + time + "] " + msg;
        System.out.println(line);
        if (logWriter != null) {
            logWriter.println(line);
        }
    }

    
    private void connect(String host, int port) {
        if (connected) {
            System.out.println("Уже подключены. Сначала разорвите текущее соединение (закройте программу).");
            return;
        }
        try {
            socket = new Socket(host, port);
            in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream()), true);
            connected = true;
            log("Соединение установлено с " + host + ":" + port + " в " + sdf.format(new Date()));

            
            new Thread(this::readLoop).start();
        } catch (IOException e) {
            log("Ошибка подключения: " + e.getMessage());
        }
    }

    
    private void readLoop() {
        try {
            String line;
            while ((line = in.readLine()) != null) {
                log("ПОЛУЧЕНО: " + line);
                if (line.equals("###END_OF_FILE###")) {
                    log("Конец файла получен");
                }
            }
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
        log("Отключение в " + sdf.format(new Date()));
        try {
            if (socket != null) socket.close();
        } catch (IOException e) {
            
        }
        socket = null;
        in = null;
        out = null;
    }
}