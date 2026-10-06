import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.logging.*;


public class UdpClient extends JFrame {
    private static final Logger LOG = Logger.getLogger("udpclient");
    private static final DateTimeFormatter TIME = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSS");

    private static final String SETTINGS_FILE = "udp_client_settings.txt";
    private static final String DEFAULT_HOST = "127.0.0.1";
    private static final int DEFAULT_PORT = 666;

    private final JTextArea area = new JTextArea();
    private final JTextField field = new JTextField();
    private final DatagramSocket socket;      // датаграммный сокет на свободном локальном порту
    private InetAddress serverAddr;
    private int serverPort;

    private String savedHost;
    private int savedPort;

    public UdpClient() throws IOException {
        super("UDP-клиент (Tab - отправить)");
        socket = new DatagramSocket();
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
            serverAddr = InetAddress.getByName(savedHost);
            serverPort = savedPort;
            print("Автоматическое подключение к " + savedHost + ":" + savedPort);
        }

        print("Введите: троку и нажмите Tab");

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

        // поток приёма ответов сервера
        Thread receiver = new Thread(() -> {
            byte[] buf = new byte[1024];
            try {
                while (true) {
                    DatagramPacket p = new DatagramPacket(buf, buf.length);
                    socket.receive(p);
                    String s = new String(p.getData(), 0, p.getLength(), StandardCharsets.UTF_8);
                    LOG.info("RECEIVED: " + s);
                    print("< " + s);
                }
            } catch (IOException e) {
                // сокет закрыт
            }
        });
        receiver.setDaemon(true);
        receiver.start();
    }

    private void setupLog() throws IOException {
        LOG.setUseParentHandlers(false);
        FileHandler fh = new FileHandler("udp_client.log", true);
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
                serverAddr = InetAddress.getByName(p[1]);
                serverPort = Integer.parseInt(p[2]);
                print("Сервер: " + serverAddr.getHostAddress() + ":" + serverPort);
            } catch (Exception e) {
                print("Ошибка: " + e.getMessage());
            }
            return;
        }
        if (serverAddr == null) {
            print("Адрес не задан. Используйте connect <адрес> <порт>");
            return;
        }
        try {
            byte[] data = text.getBytes(StandardCharsets.UTF_8);
            socket.send(new DatagramPacket(data, data.length, serverAddr, serverPort));
            LOG.info("SENT: " + text);
            print("> " + text);
        } catch (IOException e) {
            print("Ошибка отправки: " + e.getMessage());
        }
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            try {
                new UdpClient().setVisible(true);
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
    }
}