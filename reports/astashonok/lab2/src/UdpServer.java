import java.io.*;
import java.net.*;
import java.util.HashMap;
import java.util.Map;

public class UdpServer {
    public static final int PORT = 6666;
    private static final int BUF = 4096;

    static class SaveJob {
        String name;
        StringBuilder content = new StringBuilder();
    }

    public static void main(String[] args) throws IOException {
        DatagramSocket socket = new DatagramSocket(PORT);
        System.out.println("UDP server started on " + PORT);

        Map<String, SaveJob> jobs = new HashMap<String, SaveJob>();
        byte[] buf = new byte[BUF];

        while (true) {
            try {
                DatagramPacket packet = new DatagramPacket(buf, buf.length);
                socket.receive(packet);

                String text = new String(packet.getData(), 0, packet.getLength());
                InetAddress address = packet.getAddress();
                int port = packet.getPort();
                String key = address.getHostAddress() + ":" + port;

                System.out.println("from " + key + " : " + text);

                String reply = handle(text, key, jobs);
                if (reply != null) {
                    byte[] out = reply.getBytes();
                    DatagramPacket answer = new DatagramPacket(out, out.length, address, port);
                    socket.send(answer);
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    private static String handle(String text, String key, Map<String, SaveJob> jobs) {
        SaveJob job = jobs.get(key);

        if (job != null) {
            int p = text.indexOf("#end##");
            if (p >= 0) {
                job.content.append(text.substring(0, p));
                jobs.remove(key);
                try {
                    FileWriter fw = new FileWriter(job.name);
                    fw.write(job.content.toString());
                    fw.close();
                    return "OK: file " + job.name + " created";
                } catch (IOException e) {
                    return "ERROR: cannot create file";
                }
            }
            job.content.append(text).append('\n');
            return null;
        }

        if (text.startsWith("save ")) {
            String name = text.substring(5).trim();
            if (name.isEmpty()) {
                return "ERROR: cannot create file";
            }
            SaveJob next = new SaveJob();
            next.name = name;
            jobs.put(key, next);
            return null;
        }

        return text;
    }
}
