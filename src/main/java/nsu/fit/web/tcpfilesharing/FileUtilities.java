package nsu.fit.web.tcpfilesharing;

import java.io.File;
import java.io.IOException;
import java.util.Optional;

public class FileUtilities {

    public static synchronized File createDirectory(String directoryPath) throws IOException {
        File dir = new File(directoryPath);
        if (dir.exists()) {
            return dir;
        }
        if (dir.mkdirs()) {
            return dir;
        }
        throw new IOException("Failed to create directory '" + dir.getAbsolutePath() + "' for an unknown reason.");
    }

    public static String getExtension(String filename) {
        Optional<String> optional = Optional.ofNullable(filename)
                .filter(f -> f.contains("."))
                .map(f -> f.substring(filename.lastIndexOf(".") + 1));
        return optional.isEmpty() ? null : optional.get();
    }

    public static String getBaseName(String filename) {
        Optional<String> optional = Optional.ofNullable(filename)
                .filter(f -> f.contains("."))
                .map(f -> f.substring(0, filename.lastIndexOf(".")));
        return optional.isEmpty() ? null : optional.get();
    }

    public static synchronized File getUniqueFilename(File file) {
        String baseName = getBaseName(file.getName());
        String extension = getExtension(file.getName());
        int counter = 1;
        while (file.exists()) {
            file = new File(file.getParent(), baseName + "-" + (counter++) + "." + extension);
        }
        return file;
    }

}
