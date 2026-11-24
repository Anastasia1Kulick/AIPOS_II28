import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.KeyEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class UDPClient extends JFrame {

    private static final long serialVersionUID = 1L;

    private static final String LOG_FILE = "udp_client_log.txt";
    private static final DateTimeFormatter TIME_FORMAT =
            DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSS");

    private final JTextArea outputArea = new JTextArea();
    private final JTextField inputField = new JTextField();

    // состояние (доступ только внутри synchronized(this))
    private DatagramSocket socket;     // клиентский сокет, null - если сервер не задан
    private InetAddress serverAddress; // адрес сервера
    private int serverPort;            // порт сервера

    private final Object logLock = new Object();
    private PrintWriter logWriter;

    public UDPClient() {
        super("UDP client - variant 10");
        openLogFile();
        buildGui();
        printStatus("Ready. Type: connect <host> <port>  and press End. Log file: " + LOG_FILE);
    }
    private void buildGui() {
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setSize(720, 460);
        setLocationRelativeTo(null);

        outputArea.setEditable(false);
        outputArea.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 13));

        inputField.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 14));
        // Клавиша End -> действие "отправить строку" (перекрывает стандартный переход в конец поля)
        inputField.getInputMap(JComponent.WHEN_FOCUSED)
                .put(KeyStroke.getKeyStroke(KeyEvent.VK_END, 0), "sendOnEnd");
        inputField.getActionMap().put("sendOnEnd", new AbstractAction() {
            private static final long serialVersionUID = 1L;

            @Override
            public void actionPerformed(ActionEvent e) {
                onEndPressed();
            }
        });

        JLabel hint = new JLabel(" Type a line and press End to send.  Commands: "
                + "connect <host> <port>  |  disconnect <host> <port>");

        JPanel bottom = new JPanel(new BorderLayout());
        bottom.add(hint, BorderLayout.NORTH);
        bottom.add(inputField, BorderLayout.CENTER);

        add(new JScrollPane(outputArea), BorderLayout.CENTER);
        add(bottom, BorderLayout.SOUTH);

        addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent e) {
                shutdown();
            }
        });
    }

    private void onEndPressed() {
        String line = inputField.getText();
        inputField.setText("");
        if (line.isEmpty()) {
            return;
        }
        handleLine(line);
    }
    void handleLine(String line) {
        String[] t = line.trim().split("\\s+");
        String command = t[0].toLowerCase();
        if (command.equals("connect")) {
            cmdConnect(t);
        } else if (command.equals("disconnect")) {
            cmdDisconnect(t);
        } else {
            sendToServer(line);
        }
    }
    private void cmdConnect(String[] t) {
        if (t.length != 3) {
            printStatus("Usage: connect <host> <port>");
            return;
        }
        int port = parsePort(t[2]);
        if (port < 0) {
            return;
        }
        synchronized (this) {
            if (socket != null) {
                printStatus("Server is already set: " + serverAddress.getHostAddress() + ":"
                        + serverPort + ". Use disconnect first.");
                return;
            }
        }
        try {
            InetAddress address = InetAddress.getByName(t[1]);
            // Датаграммный сокет клиента: локальный порт назначается системой автоматически
            DatagramSocket s = new DatagramSocket();
            synchronized (this) {
                socket = s;
                serverAddress = address;
                serverPort = port;
            }
            // Отдельный поток принимает ответы сервера, пока сокет не будет закрыт
            Thread receiver = new Thread(() -> receiveLoop(s), "udp-client-receiver");
            receiver.setDaemon(true);
            receiver.start();
            printStatus("Server set to " + address.getHostAddress() + ":" + port
                    + " (local port " + s.getLocalPort() + "). UDP: availability is not checked.");
        } catch (UnknownHostException e) {
            printStatus("Unknown host: " + t[1]);
        } catch (SocketException e) {
            printStatus("Cannot create socket: " + e.getMessage());
        }
    }

    private void receiveLoop(DatagramSocket s) {
        byte[] buf = new byte[65535];
        while (!s.isClosed()) {
            DatagramPacket packet = new DatagramPacket(buf, buf.length);
            try {
                s.receive(packet); // блокируется до прихода датаграммы или закрытия сокета
            } catch (IOException e) {
                if (!s.isClosed()) {
                    printStatus("Receive error: " + e.getMessage() + " (reconnect to continue)");
                }
                break; // сокет закрыт командой disconnect - нормальное завершение потока
            }
            String text = new String(packet.getData(), packet.getOffset(),
                    packet.getLength(), StandardCharsets.UTF_8);
            String time = now();
            logLine(time, "RECEIVED", text);
            appendToOutput(time + "  << " + text);
        }
    }
    private void cmdDisconnect(String[] t) {
        if (t.length != 3) {
            printStatus("Usage: disconnect <host> <port>");
            return;
        }
        int port = parsePort(t[2]);
        if (port < 0) {
            return;
        }
        DatagramSocket s;
        synchronized (this) {
            s = socket;
            if (s == null) {
                printStatus("Not connected.");
                return;
            }
            // Разрываем только если адрес и порт совпадают с заданными в connect
            if (!matches(t[1], port)) {
                printStatus("No connection to " + t[1] + ":" + port + " (current server: "
                        + serverAddress.getHostAddress() + ":" + serverPort + ")");
                return;
            }
            socket = null;
            serverAddress = null;
            serverPort = 0;
        }
        s.close(); // receive() в потоке приёма завершится исключением - поток остановится
        printStatus("Disconnected from " + t[1] + ":" + port);
    }

    // Вызывать только внутри synchronized(this). Совпадает ли адрес/порт с текущим сервером.
    private boolean matches(String host, int port) {
        if (port != serverPort) {
            return false;
        }
        if (host.equalsIgnoreCase(serverAddress.getHostAddress())) {
            return true;
        }
        try {
            return InetAddress.getByName(host).equals(serverAddress);
        } catch (UnknownHostException e) {
            return false;
        }
    }
    private void sendToServer(String text) {
        DatagramSocket s;
        InetAddress address;
        int port;
        synchronized (this) {
            s = socket;
            address = serverAddress;
            port = serverPort;
        }
        if (s == null) {
            printStatus("Not connected. Use: connect <host> <port>");
            return;
        }
        byte[] data = text.getBytes(StandardCharsets.UTF_8);
        if (data.length > 65507) {
            printStatus("Line is too long for one datagram (max 65507 bytes).");
            return;
        }
        String time = now();
        try {
            // Датаграмма содержит данные и адрес/порт получателя
            s.send(new DatagramPacket(data, data.length, address, port));
        } catch (IOException e) {
            printStatus("Send failed: " + e.getMessage());
            return;
        }
        logLine(time, "SENT    ", text);
        appendToOutput(time + "  >> " + text);
    }
    private void logLine(String time, String direction, String text) {
        synchronized (logLock) {
            if (logWriter != null) {
                logWriter.println(time + " | " + direction + " | " + text);
            }
        }
    }

    private void openLogFile() {
        try {
            logWriter = new PrintWriter(new BufferedWriter(new OutputStreamWriter(
                    new FileOutputStream(LOG_FILE, true), StandardCharsets.UTF_8)), true);
        } catch (IOException e) {
            logWriter = null;
            System.err.println("Cannot open log file: " + e.getMessage());
        }
    }

    private static String now() {
        return LocalDateTime.now().format(TIME_FORMAT);
    }

    private void printStatus(String message) {
        appendToOutput("* " + message);
    }

    private void appendToOutput(String text) {
        SwingUtilities.invokeLater(() -> {
            outputArea.append(text + "\n");
            outputArea.setCaretPosition(outputArea.getDocument().getLength());
        });
    }

    private int parsePort(String text) {
        try {
            int port = Integer.parseInt(text);
            if (port >= 1 && port <= 65535) {
                return port;
            }
        } catch (NumberFormatException ignored) {
        }
        printStatus("Invalid port: " + text + " (must be 1..65535)");
        return -1;
    }

    private void shutdown() {
        DatagramSocket s;
        synchronized (this) {
            s = socket;
            socket = null;
            serverAddress = null;
            serverPort = 0;
        }
        if (s != null) {
            s.close();
        }
        synchronized (logLock) {
            if (logWriter != null) {
                logWriter.close();
                logWriter = null;
            }
        }
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new UDPClient().setVisible(true));
    }
}
