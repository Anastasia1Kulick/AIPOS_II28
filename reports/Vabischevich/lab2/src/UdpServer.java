import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;


public class UdpServer {
    public static final int PORT = 667;
    private static final int CHAIN_LEN = 10;

    /** Состояние текущей цепочки для одного клиента. */
    private static class State { int count; int sum; }

    public static void main(String[] args) throws Exception {
        DatagramSocket socket = new DatagramSocket(PORT, InetAddress.getByName("127.0.0.1"));
        System.out.println("UDP server started on 127.0.0.1:" + PORT);
        Map<String, State> clients = new HashMap<>();
        byte[] buf = new byte[1024];
        try {
            while (true) {
                DatagramPacket packet = new DatagramPacket(buf, buf.length);
                socket.receive(packet);                       // ожидание датаграммы
                // датаграмма содержит адрес и порт отправителя
                InetAddress address = packet.getAddress();
                int port = packet.getPort();
                String key = address.getHostAddress() + ":" + port;
                State st = clients.computeIfAbsent(key, k -> new State());

                String text = new String(packet.getData(), 0, packet.getLength(), StandardCharsets.UTF_8);
                for (char ch : text.toCharArray()) {
                    if (ch == '\r' || ch == '\n') continue;
                    st.sum += ch;
                    st.count++;
                    if (st.count == CHAIN_LEN) {
                        byte[] reply = ("Checksum: " + st.sum).getBytes(StandardCharsets.UTF_8);
                        socket.send(new DatagramPacket(reply, reply.length, address, port));
                        System.out.println(key + " -> checksum " + st.sum);
                        st.count = 0;
                        st.sum = 0;
                    }
                }
            }
        } finally {
            socket.close();
        }
    }
}