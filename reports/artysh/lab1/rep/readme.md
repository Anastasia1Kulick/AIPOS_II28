<p align="center">Министерство образования Республики Беларусь</p>
<p align="center">Учреждение образования</p>
<p align="center">“Брестский Государственный технический университет”</p>
<p align="center">Кафедра ИИТ</p>
<br><br><br><br><br><br><br>
<p align="center">Лабораторная работа №1</p>
<p align="center">По дисциплине “Аппаратное и Программное обеспечение сетей”</p>
<p align="center">Тема: “Организация TCP – сервера/клиента”</p>
<p align="center">Вариант: 6</p>
<br><br><br><br><br>
<p align="right">Выполнил:</p>
<p align="right">Студент 3 курса</p>
<p align="right">Группы ИИ-28</p>
<p align="right">Артыш Е. Аю</p>
<p align="right">Проверила:</p>
<p align="right">Кулик А.Д.</p>
<br><br><br><br><br>
<p align="center">Брест 2026</p>

# Цель
Изучить основы программирования сетевых приложений на базе библиотеки java.net;
Приобрести навыки по практическому использованию библиотеки для реализации сетевых приложений на базе протоколов TCP и UDP.

# Задание на выполнение
```Изучить теоретический материал, функции и классы пакета java.net и листинг программыреализации TCP-сервера. Получить индивидуальное задание у преподавателя.```

```Разработать программу работы TCP-эхо-сервера, выполняющую функции согласноварианта задания (см. приложения). Выполнить проверку программы согласно методике, приведенной в разделе 7.1.Продемонстрировать работу системы преподавателю.```

```Представить отчет, содержащий титульный лист, листинг программы с подробнымикомментариями основных фрагментов программы.8.4 Подготовиться к защите лабораторной работы по теоретическому материалу, функциям,собственным результатам, полученным в ходе выполнения лабораторной работы```

# Задания для реализации TCP сервера
Отсылка клиенту содержимого текстового файла <имя файла> в случае приема сервером в потокесимволов команды loadfname.txt, <имя файла> – имя некоторого текстового файла, находящегосяв каталоге сервера. В случае, если запрашиваемый файл отсутствует в каталоге сервера, сервердолжен отослать сообщение об этом и разорвать соединение.

# Задания для реализации TCP клиента
В случае приема сервером в потоке символов команды create <имя файла> <текст> сервер создает файл с заданным именем и сохраняет текст. В качестве ответа сервер отсылает размер созданногофайла. Если файл не может быть создан либо его размер равен нулю, то сервер отсылаетсообщение об окончании сеанса и разрывает соединение.

# Структура программы
```
reports/Artysh/lab1/
├── rep/
│   └── README.md
└── src/
    ├── FileServer.java
    ├── FileClient.java
    └── server.log (создаётся автоматически)
```
# Код программы
## Клиентская часть

```java
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;

public class TCPClient {
    private Socket socket;
    private BufferedReader in;
    private PrintWriter out;
    private PrintWriter logWriter;
    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");
    private volatile boolean connected = false;
    private final Object consoleLock = new Object();

    public static void main(String[] args) {
        new TCPClient().run();
    }

    public void run() {
        initLog();
        log("Клиент запущен");

        BufferedReader console = new BufferedReader(
                new InputStreamReader(System.in, StandardCharsets.UTF_8));
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
                            int port = Integer.parseInt(parts[2]);
                            connect(parts[1], port);
                        } catch (NumberFormatException e) {
                            System.out.println("Неверный номер порта.");
                        }
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
            logWriter = new PrintWriter(new OutputStreamWriter(
                    new FileOutputStream("client.log", true), StandardCharsets.UTF_8), true);
        } catch (IOException e) {
            System.err.println("Не удалось открыть файл протокола: " + e.getMessage());
        }
    }

    private void log(String msg) {
        String time = sdf.format(new Date());
        String line = "[" + time + "] " + msg;
        synchronized (consoleLock) {
            System.out.println(line);
        }
        if (logWriter != null) logWriter.println(line);
    }

    private void connect(String host, int port) {
        if (connected) {
            log("Переподключение: закрываем текущее соединение");
            disconnect();
        }
        try {
            socket = new Socket(host, port);
            in = new BufferedReader(new InputStreamReader(
                    socket.getInputStream(), StandardCharsets.UTF_8));
            out = new PrintWriter(new OutputStreamWriter(
                    socket.getOutputStream(), StandardCharsets.UTF_8), true);
            connected = true;
            log("Соединение установлено с " + host + ":" + port
                    + " в " + sdf.format(new Date()));
            new Thread(this::readLoop, "reader").start();
        } catch (IOException e) {
            log("Ошибка подключения: " + e.getMessage());
        }
    }


    private void readLoop() {
        try {
            String line;
            while ((line = in.readLine()) != null) {
                log("ПОЛУЧЕНО: " + line);
                if (line.equals("SESSION ENDED")) {
                    log("Сервер завершил сеанс. Соединение будет закрыто.");
                    break;
                }
            }
        } catch (IOException e) {
            if (connected) log("Соединение разорвано: " + e.getMessage());
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
        } catch (IOException ignored) {
        }
        socket = null;
        in = null;
        out = null;
    }
}

```
## Серверная часть
```java
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

```