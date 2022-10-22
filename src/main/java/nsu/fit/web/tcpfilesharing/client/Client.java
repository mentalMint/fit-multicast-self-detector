package nsu.fit.web.tcpfilesharing.client;

import java.io.*;
import java.net.Socket;
import java.nio.ByteBuffer;

public class Client implements AutoCloseable{
    Socket clientSocket;
    PrintWriter out;
    BufferedReader in;

    public Client(String serverName, Integer serverPort) throws IOException {
        clientSocket = new Socket(serverName, serverPort);
        out = new PrintWriter(clientSocket.getOutputStream(), true);
        in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()));
    }

    public void send(byte[] fileContent) throws IOException {
        OutputStream output = clientSocket.getOutputStream();

//        byte[] byteFileName = fileName.getBytes(StandardCharsets.UTF_8);
//        output.write(byteFileName);

//        byte[] byteFileSize = ByteBuffer.allocate(Integer.BYTES).putInt(fileContent.length).array();
//        output.write(byteFileSize);
        output.write(fileContent);
        output.close();
    }

    public String sendMessage(String msg) throws IOException {
        out.println(msg);
        String resp = in.readLine();
        return resp;
    }

    public void stopConnection() throws IOException {
        in.close();
        out.close();
        clientSocket.close();
    }

    @Override
    public void close() throws Exception {
        clientSocket.close();
    }
}
