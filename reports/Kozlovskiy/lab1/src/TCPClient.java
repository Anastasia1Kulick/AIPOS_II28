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

public class TCPClient extends JFrame {

    private static final long serialVersionUID = 1L;

    private static final String LOG_FILE = "tcp_client_log.txt";
    private static final DateTimeFormatter TIME_FORMAT =
            DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSS");
    private static final int CONNECT_TIMEOUT_MS = 5000;

    private final JTextArea outputArea = new JTextArea();
    private final JTextField inputField = new JTextField();

    // состояние соединения (доступ только внутри synchronized(this))
    private Socket socket;          // текущее соединение, null - если не подключены
    private PrintWriter toServer;
    private boolean connecting;

    private final Object logLock = new Object();
    private PrintWriter logWriter;

    public TCPClient() {
        super("TCP client - variant 10");
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
        // Привязываем клавишу End к действию "отправить строку".
        // Привязка перекрывает стандартное действие End (перенос курсора в конец строки).
        inputField.getInputMap(JComponent.WHEN_FOCUSED)
                .put(KeyStroke.getKeyStroke(KeyEvent.VK_END, 0), "sendOnEnd");
        inputField.getActionMap().put("sendOnEnd", new AbstractAction() {
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
        String host = t[1];

        synchronized (this) {
            if (socket != null) {
                printStatus("Already connected to " + describe(socket) + ". Use disconnect first.");
                return;
            }
            if (connecting) {
                printStatus("Connection attempt is already in progress.");
                return;
            }
            connecting = true;
        }
        printStatus("Connecting to " + host + ":" + port + " ...");

        Thread reader = new Thread(() -> connectAndListen(host, port), "tcp-client-reader");
        reader.setDaemon(true);
        reader.start();
    }

    private void connectAndListen(String host, int port) {
        Socket s = new Socket();
        PrintWriter writer;
        BufferedReader reader;
        try {
            s.connect(new InetSocketAddress(host, port), CONNECT_TIMEOUT_MS);
            writer = new PrintWriter(new BufferedWriter(
                    new OutputStreamWriter(s.getOutputStream(), StandardCharsets.UTF_8)), false);
            reader = new BufferedReader(
                    new InputStreamReader(s.getInputStream(), StandardCharsets.UTF_8));
        } catch (IOException e) {
            try {
                s.close();
            } catch (IOException ignored) {
            }
            synchronized (this) {
                connecting = false;
            }
            printStatus("Cannot connect to " + host + ":" + port + " - " + e.getMessage());
            return;
        }

        synchronized (this) {
            socket = s;
            toServer = writer;
            connecting = false;
        }
        printStatus("Connected to " + describe(s));

        try {
            String line;
            while ((line = reader.readLine()) != null) {
                logReceived(line);
            }
        } catch (IOException e) {
            // сокет закрыт (нами по disconnect или из-за сетевой ошибки) - просто выходим из цикла
        }

        // Выход из цикла: соединение закончилось. Если это не наш disconnect - сообщаем.
        boolean closedByPeer;
        synchronized (this) {
            closedByPeer = (socket == s);
            if (closedByPeer) {
                socket = null;
                toServer = null;
            }
        }
        try {
            s.close();
        } catch (IOException ignored) {
        }
        if (closedByPeer) {
            printStatus("Connection closed by server / lost.");
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
        Socket s;
        synchronized (this) {
            s = socket;
            if (s == null) {
                printStatus("Not connected.");
                return;
            }
            // Разрываем соединение только если адрес и порт совпадают с текущим
            if (!matches(s, t[1], port)) {
                printStatus("No connection to " + t[1] + ":" + port
                        + " (current connection: " + describe(s) + ")");
                return;
            }
            socket = null;
            toServer = null;
        }
        try {
            s.close();
        } catch (IOException ignored) {
        }
        printStatus("Disconnected from " + t[1] + ":" + port);
    }

    // Совпадает ли адрес/порт из команды с адресом/портом сокета (имя хоста или IP).
    private static boolean matches(Socket s, String host, int port) {
        if (s.getPort() != port) {
            return false;
        }
        if (host.equalsIgnoreCase(s.getInetAddress().getHostAddress())) {
            return true;
        }
        try {
            return InetAddress.getByName(host).equals(s.getInetAddress());
        } catch (UnknownHostException e) {
            return false;
        }
    }
    private void sendToServer(String text) {
        PrintWriter w;
        synchronized (this) {
            w = toServer;
        }
        if (w == null) {
            printStatus("Not connected. Use: connect <host> <port>");
            return;
        }
        String time = now();
        w.print(text + "\n");
        w.flush();
        if (w.checkError()) {
            printStatus("Send failed - connection is broken.");
            return;
        }
        logLine(time, "SENT    ", text);
        appendToOutput(time + "  >> " + text);
    }
    private void logReceived(String line) {
        String time = now();
        logLine(time, "RECEIVED", line);
        appendToOutput(time + "  << " + line);
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

    private static String describe(Socket s) {
        return s.getInetAddress().getHostAddress() + ":" + s.getPort();
    }

    private void shutdown() {
        Socket s;
        synchronized (this) {
            s = socket;
            socket = null;
            toServer = null;
        }
        if (s != null) {
            try {
                s.close();
            } catch (IOException ignored) {
            }
        }
        synchronized (logLock) {
            if (logWriter != null) {
                logWriter.close();
                logWriter = null;
            }
        }
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new TCPClient().setVisible(true));
    }
}
