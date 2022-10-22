package nsu.fit.web.tcpfilesharing.server;

import java.io.IOException;

public class ServerMain {
    private static Integer serverPort;

    private static void parseArguments(String[] args) {
        if (args.length < 1) {
            System.err.println("Wrong amount of arguments\n");
            return;
        }
        serverPort = Integer.parseInt(args[0]);
    }

    public static void main(String[] args) {
        parseArguments(args);
        try {
            Server server = new Server(serverPort);
            server.start();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

}

