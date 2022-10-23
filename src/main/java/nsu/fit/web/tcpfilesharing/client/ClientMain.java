package nsu.fit.web.tcpfilesharing.client;

import java.io.*;
import java.nio.file.Files;

public class ClientMain {
    private static String filePath;
    private static String serverName;
    private static Integer serverPort;


    private static void parseArguments(String[] args) {
        if (args.length < 3) {
            throw new IllegalArgumentException("Wrong amount of arguments\n");
        }
        filePath = args[0];
        serverName = args[1];
        serverPort = Integer.parseInt(args[2]);
    }

    public static void main(String[] args) {
        try {
            parseArguments(args);
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
            return;
        }

        File file = new File(filePath);
        String path = file.getPath();
        System.out.println(path);

        byte[] fileContent;
        try {
            fileContent = Files.readAllBytes(file.toPath());
        } catch (IOException e) {
            e.printStackTrace();
            return;
        }

        System.out.println("Send: " + file.getName());

        try (Client client = new Client(serverName, serverPort)){
            client.start(fileContent, file.getName());
            System.out.println("Success");
        } catch (UnsuccessfulFileSharingException e) {
            System.out.println("Failure");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
