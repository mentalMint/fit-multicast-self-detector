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
import java.util.Collections;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.zip.CRC32;
import java.util.zip.CheckedInputStream;

import static java.nio.file.StandardOpenOption.*;
import static nsu.fit.web.tcpfilesharing.FileUtilities.createDirectory;
import static nsu.fit.web.tcpfilesharing.FileUtilities.getUniqueFilename;

public class Server {
    private final ServerSocket serverSocket;
    private final long timeout = 3000000;
    private ExecutorService executorService;
    Thread shutdownHook = new Thread(() -> {
        try {
            stop();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    });

    public Server(Integer serverPort) throws IOException {
        serverSocket = new ServerSocket(serverPort);
    }

    private class ReceivingTask implements Runnable {
        Socket clientSocket;
        String uniqueFileName;
        long fileSize;
        long totalBytesRead;
        private final Speedometer speedometer;

        public ReceivingTask(Socket clientSocket) {
            this.clientSocket = clientSocket;
            speedometer = new Speedometer();
        }

        @Override
        public void run() {
            try {
                System.out.println(clientSocket);
                OutputStream output = clientSocket.getOutputStream();

                if (receive(clientSocket)) {
                    System.out.println("Downloaded successfully");
                    output.write(0);
                } else {
                    System.out.println("Error while downloading");
                    output.write(1);
                }

                clientSocket.close();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }


        private void handleTimeout(long bytesRead) {
            System.out.println("Name: " + uniqueFileName);
            double totalBytesReadTmp;
            double fileSizeTmp;
            String units;
            if (fileSize > 1000 && fileSize <= 1000000) {
                fileSizeTmp = (double) fileSize / 1000;
                totalBytesReadTmp = (double) totalBytesRead / 1000;
                units = "Kb";
            } else if (fileSize > 1000000 && fileSize <= 1000000000) {
                fileSizeTmp = (double) fileSize / 1000000;
                totalBytesReadTmp = (double) totalBytesRead / 1000000;
                units = "Mb";
            } else if (fileSize > 1000000000) {
                fileSizeTmp = (double) fileSize / 1000000000;
                totalBytesReadTmp = (double) totalBytesRead / 1000000000;
                units = "Gb";

            } else {
                fileSizeTmp = fileSize;
                totalBytesReadTmp = totalBytesRead;
                units = "B";
            }

            System.out.println("Downloaded: " + totalBytesReadTmp + " " + units + "/" + fileSizeTmp + " " + units);
            System.out.println("Instant speed: " + speedometer.getInstantSpeed(bytesRead) + " B/s");
            System.out.println("Average speed: " + speedometer.getAverageSpeed(bytesRead) + " B/s");
            System.out.println("");
        }

        private byte[] readBytes(InputStream input, int size) throws IOException {
            byte[] buffer = new byte[size];
            int totalBytesRead = 0;
            int bytesRead;

            while (totalBytesRead < size) {
                try {
                    if ((bytesRead = input.read(buffer, totalBytesRead, size - totalBytesRead)) >= 0) {
                        totalBytesRead += bytesRead;
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


        private long readFile(InputStream input, long size, Path filePath) throws IOException {
            int bufferSize = 100000;
            byte[] buffer = new byte[bufferSize];
            totalBytesRead = 0;
            int bytesRead;
            long startTime = System.currentTimeMillis();

            CheckedInputStream checkedInputStream = new CheckedInputStream(input, new CRC32());
            speedometer.start();
            while (totalBytesRead < size) {
                try {
                    if (size - totalBytesRead < bufferSize) {
                        bytesRead = checkedInputStream.read(buffer, 0, (int) (size - totalBytesRead));
                    } else {
                        bytesRead = checkedInputStream.read(buffer, 0, bufferSize);
                    }
                    if (bytesRead < 0) {
                        break;
                    }

                    if (speedometer.isEnabled()) {
                        handleTimeout(totalBytesRead);
                        speedometer.disable();
                    }
                    if (!speedometer.isEnabled() && speedometer.getTimeInterval() >= timeout * 1000) {
                        speedometer.enable();
                    }

                    totalBytesRead += bytesRead;
                    Files.write(filePath, Arrays.copyOfRange(buffer, 0, bytesRead), CREATE, WRITE, APPEND);
//                    printProgress(startTime, size, totalBytesRead); //works only with one thread
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
//            System.out.println("Name size: " + nameSize);

            fileSize = readLong(input);
//            System.out.println("File size: " + fileSize);

            String name = readName(input, nameSize);
//            System.out.println("Name: " + name);

            File uploads = createDirectory("uploads");
            String uniquePath = getUniqueFilename(new File(uploads + "/" + name)).getPath();
            Path filePath = Path.of(uniquePath);
            File uniqueFile = new File(uniquePath);
            uniqueFileName = uniqueFile.getName();

            long calculatedChecksum = readFile(input, fileSize, filePath);
//            System.out.println("\nCalculated checksum: " + calculatedChecksum);

            long receivedChecksum = readLong(input);
//            System.out.println("Received checksum: " + receivedChecksum);

            return receivedChecksum == calculatedChecksum;
        }


        private static void printProgress(long startTime, long total, long current) {
            long eta = current == 0 ? 0 :
                    (total - current) * (System.currentTimeMillis() - startTime) / current;

            String etaHms = current == 0 ? "N/A" :
                    String.format("%02d:%02d:%02d", TimeUnit.MILLISECONDS.toHours(eta),
                            TimeUnit.MILLISECONDS.toMinutes(eta) % TimeUnit.HOURS.toMinutes(1),
                            TimeUnit.MILLISECONDS.toSeconds(eta) % TimeUnit.MINUTES.toSeconds(1));

            StringBuilder string = new StringBuilder(140);
            int percent = (int) (current * 100 / total);
            string
                    .append("\f\r")
                    .append(String.join("", Collections.nCopies(percent == 0 ? 2 : 2 - (int) (Math.log10(percent)), " ")))
                    .append(String.format(" %d%% [", percent))
                    .append(String.join("", Collections.nCopies(percent / 2, "=")))
                    .append('>')
                    .append(String.join("", Collections.nCopies((100 - percent + 1) / 2, " ")))
                    .append(']')
                    .append(String.join("", Collections.nCopies((int) (Math.log10(total)) - (int) (Math.log10(current)), " ")))
                    .append(String.format(" %d/%d, ETA: %s", current, total, etaHms));

            System.out.print(string);
        }
    }

    private void stop() throws IOException {
        if (executorService != null) {
            executorService.shutdown();
        }
        serverSocket.close();
    }

    public void start() throws IOException {
        executorService = Executors.newCachedThreadPool();
        Runtime.getRuntime().addShutdownHook(shutdownHook);

        while (true) {
            try {
                Socket clientSocket = serverSocket.accept();
                ReceivingTask task = new ReceivingTask(clientSocket);
                executorService.submit(task);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
}
