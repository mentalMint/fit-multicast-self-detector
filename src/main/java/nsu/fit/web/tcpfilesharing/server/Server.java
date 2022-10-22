package nsu.fit.web.tcpfilesharing.server;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Optional;

import static java.nio.file.StandardOpenOption.*;

public class Server {
    private final ServerSocket serverSocket;

    public Server(Integer serverPort) throws IOException {
        serverSocket = new ServerSocket(serverPort);
    }

    public File createDirectory(String directoryPath) throws IOException {
        File dir = new File(directoryPath);
        if (dir.exists()) {
            return dir;
        }
        if (dir.mkdirs()) {
            return dir;
        }
        throw new IOException("Failed to create directory '" + dir.getAbsolutePath() + "' for an unknown reason.");
    }

    public String getExtension(String filename) {
        Optional<String> optional = Optional.ofNullable(filename)
                .filter(f -> f.contains("."))
                .map(f -> f.substring(filename.lastIndexOf(".") + 1));
        return optional.isEmpty() ? null : optional.get();
    }

    public String getBaseName(String filename) {
        Optional<String> optional = Optional.ofNullable(filename)
                .filter(f -> f.contains("."))
                .map(f -> f.substring(0, filename.lastIndexOf(".") + 1));
        return optional.isEmpty() ? null : optional.get();
    }


    private File getUniqueFilename(File file) {
        String baseName = getBaseName(file.getName());
        String extension = getExtension(file.getName());
        int counter = 1;
        while (file.exists()) {
            file = new File(file.getParent(), baseName + "-" + (counter++) + "." + extension);
        }
        return file;
    }

    public void start() throws IOException {
        createDirectory("uploads");
        while (true) {
            try {
                Socket clientSocket = serverSocket.accept();
                System.out.println(clientSocket);
//                PrintWriter out = new PrintWriter(clientSocket.getOutputStream(), true);
//                BufferedReader in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()));

//                String inputLine;
//                while ((inputLine = in.readLine()) != null) {
//                    if ("hello server".equals(inputLine)) {
//                        out.println("hello client");
//                    }
//                    else {
//                        out.println("unrecognised greeting");
//                    }
//                    System.out.println(inputLine);
//                }

                InputStream input = clientSocket.getInputStream();
                byte[] content = new byte[16];
                int length;
//                int totalLength = 0;
//                int fileSize;
//                byte[] byteFileSize = new byte[Integer.BYTES];
                while ((length = input.read(content)) >= 0) {
//                    totalLength += length;
//                    if (totalLength <= Integer.BYTES) {
//                        byteFi
//                    }
                    System.out.println(Arrays.toString(content));
                    Files.write(Path.of("uploads/example.txt"), Arrays.copyOfRange(content, 0, length), CREATE, WRITE, APPEND);
                }

                clientSocket.close();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private byte[] receive(InputStream input) throws IOException {
        byte[] content = new byte[16];
        input.read(content);
        return content;
    }
}
