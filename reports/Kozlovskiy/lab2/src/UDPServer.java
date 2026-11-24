import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

public class UDPServer {

    public static final int PORT = 666;
    public static final int CHUNK_SIZE = 10;

    public static void main(String[] args) throws IOException {
        int port = (args.length > 0) ? Integer.parseInt(args[0]) : PORT;

        Map<String, StringBuilder> pending = new HashMap<>();
        byte[] buf = new byte[65535];

        DatagramSocket socket = new DatagramSocket(port);
        System.out.println("UDP server started on port " + socket.getLocalPort());
        try {
            while (true) {
                DatagramPacket packet = new DatagramPacket(buf, buf.length);
                socket.receive(packet);

                String client = packet.getAddress().getHostAddress() + ":" + packet.getPort();
                StringBuilder chunk = pending.computeIfAbsent(client, k -> new StringBuilder());

                for (int i = 0; i < packet.getLength(); i++) {
                    int b = buf[i] & 0xFF;
                    if (b == '\r' || b == '\n' || b == 0) {
                        continue;
                    }
                    chunk.append((char) b);

                    if (chunk.length() == CHUNK_SIZE) {
                        String answer = "Checksum of [" + printable(chunk) + "] = " + checksum(chunk);
                        byte[] data = answer.getBytes(StandardCharsets.US_ASCII);
                        // ответ уходит на адрес и порт отправителя
                        DatagramPacket reply = new DatagramPacket(
                                data, data.length, packet.getAddress(), packet.getPort());
                        try {
                            socket.send(reply);
                        } catch (IOException e) {
                            System.out.println("[" + client + "] send error: " + e.getMessage());
                        }
                        System.out.println("[" + client + "] " + answer);
                        chunk.setLength(0);
                    }
                }
            }
        } finally {
            socket.close();
        }
    }

    // сумма ASCII-кодов символов
    private static int checksum(CharSequence chars) {
        int sum = 0;
        for (int i = 0; i < chars.length(); i++) {
            sum += chars.charAt(i);
        }
        return sum;
    }

    // непечатные и не-ASCII символы заменяются на '?' только для вывода
    private static String printable(CharSequence chars) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < chars.length(); i++) {
            char c = chars.charAt(i);
            sb.append((c >= 32 && c < 127) ? c : '?');
        }
        return sb.toString();
    }
}
