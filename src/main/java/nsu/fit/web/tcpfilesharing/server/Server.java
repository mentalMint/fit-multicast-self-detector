package nsu.fit.web.tcpfilesharing.server;

import nsu.fit.web.tcpfilesharing.Speedometer;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Optional;
import java.util.zip.CRC32;
import java.util.zip.CheckedInputStream;

import static java.nio.file.StandardOpenOption.*;

public class Server {
    private final ServerSocket serverSocket;
    private final Speedometer speedometer;
    private final long timeout = 3000000;

    public Server(Integer serverPort) throws IOException {
        serverSocket = new ServerSocket(serverPort);
        speedometer = new Speedometer();
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
                .map(f -> f.substring(0, filename.lastIndexOf(".")));
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

    private void handleTimeout(int bytesRead) {
        System.out.println("Instant speed: " + speedometer.getInstantSpeed(bytesRead) + " B/s");
    }

    private byte[] readBytes(InputStream input, int size) throws IOException {
        byte[] buffer = new byte[size];
        int totalBytesRead = 0;
        int bytesRead;

        while (totalBytesRead < size) {
            try {
                if ((bytesRead = input.read(buffer, totalBytesRead, size - totalBytesRead)) >= 0) {
                    totalBytesRead += bytesRead;

//                    if (speedometer.isEnabled() && speedometer.getTimeInterval() >= timeout * 10) {
//                        handleTimeout(totalBytesRead);
////                        System.err.println("timeout");
//                        speedometer.disable();
//                    }
//
//                    if (!speedometer.isEnabled() && speedometer.getTimeInterval() >= timeout * 1000) {
//                        speedometer.enable();
//                    }

                } else {
                    break;
                }
            } catch (SocketTimeoutException e) {
                handleTimeout(totalBytesRead);
            }
        }
        return buffer;
    }

    private int readInt(InputStream input) throws IOException {
        byte[] bytes = readBytes(input, Integer.BYTES);
        ByteBuffer buffer = ByteBuffer.allocate(Integer.BYTES);
        buffer.put(bytes);
        buffer.rewind();
        return buffer.getInt();
    }

    private long readLong(InputStream input) throws IOException {
        byte[] bytes = readBytes(input, Long.BYTES);
        ByteBuffer buffer = ByteBuffer.allocate(Long.BYTES);
        buffer.put(bytes);
        buffer.rewind();
        return buffer.getLong();
    }

    private String readName(InputStream input, int size) throws IOException {
        return new String(readBytes(input, size), StandardCharsets.UTF_8);
    }

    private long readFile(InputStream input, int size, Path filePath) throws IOException {
        int bufferSize = 100000;
        byte[] buffer = new byte[bufferSize];
        int totalBytesRead = 0;
        int bytesRead;

        CheckedInputStream checkedInputStream = new CheckedInputStream(input, new CRC32());
        speedometer.start();
        while (totalBytesRead < size) {
            try {
                if (size - totalBytesRead < bufferSize) {
                    bytesRead = checkedInputStream.read(buffer, 0, size - totalBytesRead);
                } else {
                    bytesRead = checkedInputStream.read(buffer, 0, bufferSize);
                }
                if (bytesRead < 0) {
                    break;
                }

                if (speedometer.isEnabled() && speedometer.getTimeInterval() >= timeout * 10) {
                    handleTimeout(totalBytesRead);
//                        System.err.println("timeout");
                    speedometer.disable();
                }

                if (!speedometer.isEnabled() && speedometer.getTimeInterval() >= timeout * 100) {
                    speedometer.enable();
                }

                totalBytesRead += bytesRead;
                Files.write(filePath, Arrays.copyOfRange(buffer, 0, bytesRead), CREATE, WRITE, APPEND);
            } catch (SocketTimeoutException e) {
                handleTimeout(totalBytesRead);
            }
        }
        return checkedInputStream.getChecksum().getValue();
    }

    private boolean receive(Socket clientSocket) throws IOException {
        InputStream input = clientSocket.getInputStream();
        clientSocket.setSoTimeout((int) timeout);

        int nameSize = readInt(input);
        System.out.println("Name size: " + nameSize);

        int fileSize = readInt(input);
        System.out.println("File size: " + fileSize);

        long receivedChecksum = readLong(input);
        System.out.println("Received checksum: " + receivedChecksum);

        String name = readName(input, nameSize);
        System.out.println("Name: " + name);

        File uploads = createDirectory("uploads");
        Path filePath = Path.of(getUniqueFilename(new File(uploads + "/" + name)).getPath());
        long calculatedChecksum = readFile(input, fileSize, filePath);
        System.out.println("Calculated checksum: " + calculatedChecksum);

        return receivedChecksum == calculatedChecksum;
    }

    public void start() throws IOException {
        while (true) {
            try {
                Socket clientSocket = serverSocket.accept();
                System.out.println(clientSocket);
                OutputStream output = clientSocket.getOutputStream();

                if (receive(clientSocket)) {
                    System.out.println("Nice");
                    output.write(0);
                } else {
                    System.out.println("Bad");
                    output.write(1);
                }

                clientSocket.close();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }
}
