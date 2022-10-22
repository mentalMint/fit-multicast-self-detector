package nsu.fit.web.tcpfilesharing.client;

import nsu.fit.web.tcpfilesharing.UnicodeReader;

import java.io.*;
import java.net.URISyntaxException;
import java.net.URL;
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

//        URL url;
//        try {
//            url = ClientMain.class.getResource(filePath);
//        } catch (NullPointerException e) {
//            System.err.println("Name is not provided\n");
//            return;
//        }
//
//        if (url == null) {
//            System.err.println("File not found\n");
//            return;
//        }

        byte[] fileContent;
        try {
            fileContent = Files.readAllBytes(file.toPath());
        } catch (IOException e) {
            e.printStackTrace();
            return;
        }

//        try {
//            fileContent = UnicodeReader.readToBytes(url);
//        } catch (IOException | URISyntaxException e) {
//            e.printStackTrace();
//            return;
//        }

        System.out.println(file.getName());

        try (Client client = new Client(serverName, serverPort)){
            client.send(fileContent);
//            String response = client.sendMessage("hello server");
//            System.out.println(response);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

}
