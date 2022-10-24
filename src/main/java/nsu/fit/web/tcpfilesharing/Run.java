package nsu.fit.web.tcpfilesharing;

import nsu.fit.web.tcpfilesharing.client.ClientMain;

public class Run {
    private static String[] clientArgs = {"src/main/resources/RootmanTrailer.mp4", "localhost", "12000"};

    public static void main(String[] args) {
        for (int i = 0; i < 10; i++) {
            ClientMain.main(clientArgs);
        }
    }
}
