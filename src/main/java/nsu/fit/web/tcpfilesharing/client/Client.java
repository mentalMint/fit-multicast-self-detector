package nsu.fit.web.tcpfilesharing.client;

import java.io.*;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.zip.CRC32;
import java.util.zip.CheckedInputStream;
import java.util.zip.Checksum;

public class Client implements AutoCloseable{
    Socket clientSocket;

    public Client(String serverName, Integer serverPort) throws IOException {
        clientSocket = new Socket(serverName, serverPort);
    }

    public static long getCRC32Checksum(byte[] bytes) {
        Checksum crc32 = new CRC32();
        crc32.update(bytes, 0, bytes.length);
        return crc32.getValue();
    }

    private long sendFile(OutputStream output, File file) throws IOException {
        int bufferSize = 100000;
        byte[] buffer = new byte[bufferSize];
        int bytesRead;


        FileInputStream input = new FileInputStream(file);
        CheckedInputStream checkedInputStream = new CheckedInputStream(input, new CRC32());

        while ((bytesRead = checkedInputStream.read(buffer)) >= 0) {
            output.write(Arrays.copyOfRange(buffer, 0, bytesRead));
        }
        checkedInputStream.close();
        return checkedInputStream.getChecksum().getValue();
    }

    private void send(File file) throws IOException {
        String fileName = file.getName();
        System.out.println("Send: " + fileName);
        OutputStream output = clientSocket.getOutputStream();
        byte[] byteFileName = fileName.getBytes(StandardCharsets.UTF_8);
        byte[] byteNameSize = ByteBuffer.allocate(Integer.BYTES).putInt(byteFileName.length).array();
        long fileSize = file.length();
        byte[] byteFileSize = ByteBuffer.allocate(Long.BYTES).putLong(fileSize).array();

        output.write(byteNameSize);
        output.write(byteFileSize);
        output.write(byteFileName);

        long checksum = sendFile(output, file);
        System.out.println("Checksum: " + checksum);
        byte[] byteChecksum = ByteBuffer.allocate(Long.BYTES).putLong(checksum).array();
        output.write(byteChecksum);
    }

    private void receive() throws IOException, UnsuccessfulFileSharingException {
        InputStream input = clientSocket.getInputStream();
        byte[] received = new byte[1];
        byte successful = 0;
        if (input.readNBytes(received, 0, 1) == 0 || received[0] != successful) {
            throw new UnsuccessfulFileSharingException();
        }
    }

    public void start(String filePath) throws IOException, UnsuccessfulFileSharingException {
        File file = new File(filePath);
        String path = file.getPath();
        System.out.println(path);
        send(file);
        receive();
    }

    @Override
    public void close() throws Exception {
        clientSocket.close();
    }
}
