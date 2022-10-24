package nsu.fit.web.tcpfilesharing;

public class Speedometer {
    private long startTime = 0;
    private long currentTime = 0;
    private long previousTime = 0;
    private long currentCount = 0;

    private boolean enabled = false;

    public Speedometer() {
    }

    public void start() {
        currentTime = System.nanoTime();
        previousTime = currentTime;
        startTime = currentTime;
        enabled = true;
    }

    public long getCurrentTime() {
        currentTime = System.nanoTime();
        return currentTime;
    }

    public long getPreviousTime() {
        return previousTime;
    }

    public long getTimeInterval() {
        return getCurrentTime() - previousTime;
    }

    public void mark() {
        previousTime = currentTime;
        currentTime = System.nanoTime();
    }

    public double getInstantSpeed(long count) {
        long previousCount = currentCount;
        currentCount = count;
        long time = getTimeInterval();
        double instantSpeed = (double) (currentCount - previousCount) / (time / Math.pow(10, 9));
        previousTime = currentTime;
        return instantSpeed;
    }

    public boolean isEnabled() {
        return enabled;
    }

    public void disable() {
        enabled = false;
    }

    public void enable() {
        enabled = true;
    }

    public double getAverageSpeed(long count) {
        currentCount = count;
        long time = getCurrentTime() - startTime;
        double averageSpeed = (double) currentCount / (time / Math.pow(10, 9));
        previousTime = currentTime;
        return averageSpeed;
    }
}
