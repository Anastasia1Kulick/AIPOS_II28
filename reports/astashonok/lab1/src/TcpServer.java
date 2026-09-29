import java.io.*;
import java.net.*;


class ServeOne extends Thread {
    private Socket socket;
    private BufferedReader in;
    private PrintWriter out;

    public ServeOne(Socket s) throws IOException {
        socket = s;
        in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
        out = new PrintWriter(new BufferedWriter(
                new OutputStreamWriter(socket.getOutputStream())), true);
        start();
    }

    public void run() {
        try {
            out.println("Hello, Student!");
            String line;
            while ((line = in.readLine()) != null) {
                if (line.startsWith("save ")) {
                    String name = line.substring(5).trim();
                    StringBuilder content = new StringBuilder();
                    String part;
                    boolean ok = false;
                    while ((part = in.readLine()) != null) {
                        int p = part.indexOf("#end##");
                        if (p >= 0) {
                            content.append(part.substring(0, p));
                            ok = true;
                            break;
                        }
                        content.append(part).append('\n');
                    }
                    if (!ok) break;
                    try {
                        FileWriter fw = new FileWriter(name);
                        fw.write(content.toString());
                        fw.close();
                        out.println("OK: file " + name + " created");
                    } catch (IOException e) {
                        out.println("ERROR: cannot create file");
                        break;
                    }
                } else {
                    out.println(line);
                }
            }
        } catch (IOException e) {
            System.err.println("IO Exception");
        } finally {
            try {
                socket.close();
            } catch (IOException e) {
                System.err.println("Socket not closed");
            }
        }
    }
}

public class TcpServer {
    static final int PORT = 8080;

    public static void main(String[] args) throws IOException {
        ServerSocket s = new ServerSocket(PORT);
        System.out.println("TCP server started on " + PORT);
        try {
            while (true) {
                Socket socket = s.accept();
                try {
                    new ServeOne(socket);
                } catch (IOException e) {
                    socket.close();
                }
            }
        } finally {
            s.close();
        }
    }
}
