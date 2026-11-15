import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.logging.*;

public class TcpClient extends JFrame {
    private static final Logger LOG = Logger.getLogger("tcpclient");
    private static final DateTimeFormatter TIME = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSS");

    private static final String SETTINGS_FILE = "tcp_client_settings.txt";
    private static final String DEFAULT_HOST = "127.0.0.1";
    private static final int DEFAULT_PORT = 8080;

    private final JTextArea area = new JTextArea();
    private final JTextField field = new JTextField();
    private Socket socket;
    private PrintWriter out;

    private String savedHost;
    private int savedPort;

    public TcpClient() throws IOException {
        super("TCP-клиент (Tab - отправить)");
        setupLog();
        area.setEditable(false);
        setLayout(new BorderLayout());
        add(new JScrollPane(area), BorderLayout.CENTER);
        add(field, BorderLayout.SOUTH);
        setSize(520, 380);
        setDefaultCloseOperation(EXIT_ON_CLOSE);

        // Загружаем сохраненные настройки
        loadSettings();

        // Автоподключение при старте
        if (savedHost != null) {
            try {
                connect(savedHost, savedPort);
                print("Автоматическое подключение к " + savedHost + ":" + savedPort);
            } catch (IOException e) {
                print("Ошибка автоподключения: " + e.getMessage() + ". Введите: connect <адрес> <порт>");
                savedHost = null;
            }
        }

        print("Введите: connect <адрес> <порт>, затем строку и нажмите Tab");

        // отключаем стандартное поведение Tab (переход фокуса),
        // чтобы нажатие Tab доходило до нашего обработчика
        field.setFocusTraversalKeysEnabled(false);
        field.addKeyListener(new KeyAdapter() {
            @Override
            public void keyPressed(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.VK_TAB) {   // отправка по Tab
                    e.consume();
                    processInput();
                }
            }
        });

        // гарантированно ставим курсор в поле ввода после открытия окна
        addWindowListener(new WindowAdapter() {
            @Override
            public void windowOpened(WindowEvent e) {
                field.requestFocusInWindow();
            }
        });
    }

    /** Настройка журнала: файл tcp_client.log, формат "[время] сообщение". */
    private void setupLog() throws IOException {
        LOG.setUseParentHandlers(false);
        FileHandler fh = new FileHandler("tcp_client.log", true);
        fh.setEncoding("UTF-8");
        fh.setFormatter(new Formatter() {
            @Override
            public String format(LogRecord r) {
                return "[" + LocalDateTime.now().format(TIME) + "] " + r.getMessage() + System.lineSeparator();
            }
        });
        LOG.addHandler(fh);
    }

    /** Загрузка сохраненных настроек хоста и порта. */
    private void loadSettings() {
        File file = new File(SETTINGS_FILE);
        if (!file.exists()) {
            savedHost = DEFAULT_HOST;
            savedPort = DEFAULT_PORT;
            return;
        }
        try (BufferedReader br = new BufferedReader(new FileReader(file))) {
            savedHost = br.readLine();
            try {
                savedPort = Integer.parseInt(br.readLine());
            } catch (NumberFormatException e) {
                savedPort = DEFAULT_PORT;
            }
        } catch (IOException e) {
            savedHost = DEFAULT_HOST;
            savedPort = DEFAULT_PORT;
        }
    }

    /** Сохранение текущего хоста и порта. */
    private void saveSettings() {
        try (PrintWriter pw = new PrintWriter(new FileWriter(SETTINGS_FILE))) {
            pw.print(savedHost);
            pw.println();
            pw.print(savedPort);
        } catch (IOException e) {
            // Игнорируем ошибки сохранения
        }
    }

    private void print(String s) {
        SwingUtilities.invokeLater(() -> area.append(s + "\n"));
    }

    /** Разбор введённой строки: команда connect или обычная строка для сервера. */
    private void processInput() {
        String text = field.getText().trim();
        field.setText("");
        if (text.isEmpty()) return;
        if (text.startsWith("connect")) {
            String[] p = text.split("\\s+");
            if (p.length != 3) {
                print("Формат: connect <адрес> <порт>");
                return;
            }
            try {
                savedHost = p[1];
                savedPort = Integer.parseInt(p[2]);
                saveSettings();
                connect(p[1], Integer.parseInt(p[2]));
            } catch (Exception e) {
                print("Ошибка подключения: " + e.getMessage());
            }
            return;
        }
        if (socket == null || socket.isClosed()) {
            print("Нет соединения. Используйте connect <адрес> <порт>");
            return;
        }
        out.println(text);                              // отправка серверу
        LOG.info("SENT: " + text);                      // протокол: строка и время передачи
        print("> " + text);
    }

    private void connect(String host, int port) throws IOException {
        if (socket != null) socket.close();
        socket = new Socket();
        socket.connect(new InetSocketAddress(host, port), 3000);
        out = new PrintWriter(new BufferedWriter(
                new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8)), true);
        BufferedReader in = new BufferedReader(
                new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
        print("Подключено к " + host + ":" + port);
        // Сохраняем настройки
        savedHost = host;
        savedPort = port;
        saveSettings();

        // отдельный поток принимает сообщения сервера
        Thread reader = new Thread(() -> {
            try {
                String line;
                while ((line = in.readLine()) != null) {
                    LOG.info("RECEIVED: " + line);
                    print("< " + line);
                }
            } catch (IOException e) {
                // сокет закрыт
            }
            print("Соединение закрыто");
        });
        reader.setDaemon(true);
        reader.start();
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            try {
                new TcpClient().setVisible(true);
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
    }
}