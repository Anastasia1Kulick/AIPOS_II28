import java.io.*;
import java.net.*;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class Client {
    public static final String HOST = "127.0.0.1";
    public static final int PORT = 9090;

    private static PrintWriter logWriter;
    private static DatagramSocket socket;
    private static InetAddress serverAddress;
    private static final DateTimeFormatter dtf =
            DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

    public static void main(String[] args) {
        try {
            logWriter = new PrintWriter(new FileWriter("protocol.txt", true), true);
        } catch (IOException e) {
            System.err.println("Log file creation error: " + e.getMessage());
        }

        JFrame frame = new JFrame("UDP Client (Send via PgDn)");
        JTextArea textArea = new JTextArea();
        textArea.setEditable(false);
        JTextField textField = new JTextField();

        frame.add(new JScrollPane(textArea), BorderLayout.CENTER);
        frame.add(textField, BorderLayout.SOUTH);
        frame.setSize(500, 400);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setVisible(true);

        try {
            socket = new DatagramSocket();
            serverAddress = InetAddress.getByName(HOST);
            log("CONNECTION STARTED with " + HOST + ":" + PORT);
            textArea.append("Connected! Enter a command and press PgDn to send.\n");

            new Thread(() -> {
                try {
                    byte[] buf = new byte[1024];
                    while (true) {
                        DatagramPacket packet = new DatagramPacket(buf, buf.length);
                        socket.receive(packet);
                        String line = new String(packet.getData(), 0, packet.getLength()).trim();
                        log("RECEIVED: " + line);
                        textArea.append("Server: " + line + "\n");
                        textArea.setCaretPosition(textArea.getDocument().getLength());
                    }
                } catch (IOException e) {
                    textArea.append("Connection closed.\n");
                }
            }).start();
        } catch (IOException e) {
            textArea.append("Connection error: " + e.getMessage() + "\n");
            return;
        }

        Action sendAction = new AbstractAction() {
            @Override
            public void actionPerformed(ActionEvent e) {
                String text = textField.getText();
                if (text.isEmpty()) return;

                try {
                    byte[] data = (text + "\n").getBytes();
                    DatagramPacket packet = new DatagramPacket(data, data.length, serverAddress, PORT);
                    socket.send(packet);

                    log("SENT: " + text);
                    textArea.append("You: " + text + "\n");
                    textField.setText("");

                    if (text.equals("exit")) {
                        closeConnection();
                        System.exit(0);
                    }
                } catch (IOException ex) {
                    ex.printStackTrace();
                }
            }
        };

        textField.getInputMap(JComponent.WHEN_FOCUSED)
                .put(KeyStroke.getKeyStroke(KeyEvent.VK_PAGE_DOWN, 0), "send");
        textField.getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW)
                .put(KeyStroke.getKeyStroke(KeyEvent.VK_PAGE_DOWN, 0), "send");
        textField.getActionMap().put("send", sendAction);

        frame.addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent e) {
                closeConnection();
            }
        });

        textField.requestFocusInWindow();
    }

    private static void log(String message) {
        String time = dtf.format(LocalDateTime.now());
        String logEntry = "[" + time + "] " + message;
        System.out.println(logEntry);
        if (logWriter != null) {
            logWriter.println(logEntry);
        }
    }

    private static void closeConnection() {
        try {
            log("CONNECTION ENDED");
            if (socket != null && !socket.isClosed()) socket.close();
            if (logWriter != null) logWriter.close();
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }
}