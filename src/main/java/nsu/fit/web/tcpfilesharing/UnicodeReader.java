package nsu.fit.web.tcpfilesharing;

import java.io.*;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.nio.file.Paths;

public class UnicodeReader {
    static public String read(String fileName) throws IOException {
        Path path = Paths.get(fileName);
        String content = Files.readString(path, StandardCharsets.UTF_8);
        System.out.println(content);
        return content;
    }

    static public String read(URL url) throws InvalidPathException, IOException, URISyntaxException {
        Path path = Paths.get(url.toURI());
        String content = Files.readString(path, StandardCharsets.UTF_8);
        System.out.println(content);
        return content;
    }

    static public byte[] readToBytes(URL url) throws URISyntaxException, IOException {
        Path path = Paths.get(url.toURI());
        return Files.readAllBytes(path);
    }

    public static void readUnicodeClassic(File file) {
        try (FileInputStream fis = new FileInputStream(file);
             InputStreamReader isr = new InputStreamReader(fis, StandardCharsets.UTF_8);
             BufferedReader reader = new BufferedReader(isr)
        ) {

            String str;
            while ((str = reader.readLine()) != null) {
                System.out.println(str);
            }

        } catch (IOException e) {
            e.printStackTrace();
        }

    }
}
