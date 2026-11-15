<p align="center">Министерство образования Республики Беларусь</p>
<p align="center">Учреждение образования</p>
<p align="center">“Брестский Государственный технический университет”</p>
<p align="center">Кафедра ИИТ</p>
<br><br><br><br><br><br><br>
<p align="center">Лабораторная работа №2</p>
<p align="center">По дисциплине “Аппаратное и Программное обеспечение сетей”</p>
<p align="center">Тема: “Организация UDP – сервера/клиента”</p>
<p align="center">Вариант: 6</p>
<br><br><br><br><br>
<p align="right">Выполнил:</p>
<p align="right">Студент 3 курса</p>
<p align="right">Группы ИИ-28</p>
<p align="right">Артыш Е. А.</p>
<p align="right">Проверила:</p>
<p align="right">Кулик А.Д.</p>
<br><br><br><br><br>
<p align="center">Брест 2026</p>



# Цель
Изучить основы программирования сетевых приложений на базе библиотеки java.net;
Приобрести навыки по практическому использованию библиотеки для реализации сетевых приложений на базе протоколов TCP и UDP.

# Задание на выполнение
```Изучить теоретический материал, функции и классы пакета java.net и листинг программыреализации UDP-сервера. Получить индивидуальное задание у преподавателя.```

```Разработать программу работы UDP-эхо-сервера, выполняющую функции согласноварианта задания (см. приложения). Выполнить проверку программы согласно методике, приведенной в разделе 7.1.Продемонстрировать работу системы преподавателю.```

```Представить отчет, содержащий титульный лист, листинг программы с подробнымикомментариями основных фрагментов программы.8.4 Подготовиться к защите лабораторной работы по теоретическому материалу, функциям,собственным результатам, полученным в ходе выполнения лабораторной работы```

# Задания для реализации UDP сервера
Отсылка клиенту содержимого текстового файла <имя файла> в случае приема сервером в потокесимволов команды loadfname.txt, <имя файла> – имя некоторого текстового файла, находящегосяв каталоге сервера. В случае, если запрашиваемый файл отсутствует в каталоге сервера, сервердолжен отослать сообщение об этом и разорвать соединение.

# Задания для реализации UDP клиента
В случае приема сервером в потоке символов команды create <имя файла> <текст> сервер создаетфайл с заданным именем и сохраняет текст. В качестве ответа сервер отсылает размер созданногофайла. Если файл не может быть создан либо его размер равен нулю, то сервер отсылаетсообщение об окончании сеанса и разрывает соединение.

# Структура программы
```
reports/Artysh/lab1/
├── rep/
│   └── README.md
└── src/
    ├── UDPClient.java
    ├── UDPServer.java
    └── server.log (создаётся автоматически)
```
# Код программы
## Клиентская часть

```java
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
```
## Серверная часть
```java
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
```