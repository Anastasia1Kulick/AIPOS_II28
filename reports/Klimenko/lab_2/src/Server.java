import java.io.*;
import java.net.*;

public class Server {
    public static final int PORT = 9090;

    public static void main(String args[]) {
        try (DatagramSocket socket = new DatagramSocket(PORT)) {
            System.out.println("UDP Server started: " + socket);
            
            byte[] buf = new byte[1024];

            while (true) {
                DatagramPacket packet = new DatagramPacket(buf, buf.length);
                socket.receive(packet);

                String received = new String(packet.getData(), 0, packet.getLength()).trim();
                InetAddress clientAddress = packet.getAddress();
                int clientPort = packet.getPort();

                System.out.println("Received from " + clientAddress + ":" + clientPort + " -> " + received);

                if (received.startsWith("load ")) {
                    String targetFileName = received.substring(5).trim();
                    File file = new File(targetFileName);

                    if (file.exists() && file.isFile()) {
                        try (BufferedReader fileReader = new BufferedReader(new FileReader(file))) {
                            String fileLine;
                            while ((fileLine = fileReader.readLine()) != null) {
                                byte[] data = (fileLine + "\n").getBytes();
                                DatagramPacket sendPacket = new DatagramPacket(data, data.length, clientAddress, clientPort);
                                socket.send(sendPacket);
                            }

                            byte[] eofData = "--- EOF ---\n".getBytes();
                            socket.send(new DatagramPacket(eofData, eofData.length, clientAddress, clientPort));
                        }
                    } else {
                        String errMsg = "Error: file '" + targetFileName + "' not found on server!\n";
                        byte[] data = errMsg.getBytes();
                        socket.send(new DatagramPacket(data, data.length, clientAddress, clientPort));
                        System.out.println("File not found. Client notified.");
                    }
                } else if (received.equals("exit")) {
                    System.out.println("Client requested exit.");
                    String msg = "Connection closed.\n";
                    byte[] data = msg.getBytes();
                    socket.send(new DatagramPacket(data, data.length, clientAddress, clientPort));
                }
            }
        } catch (IOException e) {
            System.err.println("UDP Server error: " + e.getMessage());
        }
    }
}