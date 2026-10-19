import java.io.*;
import java.net.*;

public class Server {
    public static final int PORT = 9090;

    public static void main(String args[]) throws IOException {
        ServerSocket s = new ServerSocket(PORT);
        System.out.println("Started: " + s);
        
        try {
            Socket socket = s.accept();
            try {
                System.out.println("Connection accepted: " + socket);
                
                BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                PrintWriter out = new PrintWriter(new BufferedWriter(new OutputStreamWriter(socket.getOutputStream())), true);
                
                String str;
                while ((str = in.readLine()) != null) {
                    if (str.startsWith("load ")) {
                        String targetFileName = str.substring(5).trim();
                        File file = new File(targetFileName);

                        if (file.exists() && file.isFile()) {
                            try (BufferedReader fileReader = new BufferedReader(new FileReader(file))) {
                                String fileLine;
                                while ((fileLine = fileReader.readLine()) != null) {
                                    out.println(fileLine);
                                }
                                out.println("--- EOF ---");
                            }
                        } else {
                            out.println("Error: file '" + targetFileName + "' not found on server!");
                            System.out.println("File not found. Closing connection as requested by task...");
                            break; 
                        }
                    } else if (str.equals("exit")) {
                        System.out.println("Closing connection...");
                        break;
                    }
                }
            } finally {
                System.out.println("Closing socket...");
                socket.close();
            }
        } catch (IOException e) {
            System.err.println("Server error: " + e.getMessage());
        } finally {
            System.out.println("Closing server...");
            s.close();
        }
    }
}