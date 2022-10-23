package nsu.fit.web.tcpfilesharing.client;

import java.io.*;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.zip.CRC32;
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

    private void send(byte[] fileContent, String fileName) throws IOException {
        OutputStream output = clientSocket.getOutputStream();
        System.err.println("here");
        byte[] byteFileName = fileName.getBytes(StandardCharsets.UTF_8);
        byte[] byteNameSize = ByteBuffer.allocate(Integer.BYTES).putInt(byteFileName.length).array();
        output.write(byteNameSize);

        byte[] byteFileSize = ByteBuffer.allocate(Integer.BYTES).putInt(fileContent.length).array();
        output.write(byteFileSize);

        long checksum = getCRC32Checksum(fileContent);
        System.out.println("Checksum: " + checksum);
        byte[] byteChecksum = ByteBuffer.allocate(Long.BYTES).putLong(checksum).array();
        output.write(byteChecksum);

        output.write(byteFileName);
        output.write(fileContent);
        output.write(byteFileName);
    }

    private void receive() throws IOException, UnsuccessfulFileSharingException {
        InputStream input = clientSocket.getInputStream();
        byte[] received = new byte[1];
        byte successful = 0;
        if (input.readNBytes(received, 0, 1) == 0 || received[0] != successful) {
            throw new UnsuccessfulFileSharingException();
        }
    }

    public void start(byte[] fileContent, String fileName) throws IOException, UnsuccessfulFileSharingException {
        send(fileContent, fileName);
        receive();
    }

    @Override
    public void close() throws Exception {
        clientSocket.close();
    }
}
