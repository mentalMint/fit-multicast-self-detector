package nsu.fit.web.tcpfilesharing.client;

public class UnsuccessfulFileSharingException extends RuntimeException {
    public UnsuccessfulFileSharingException() {
    }

    public UnsuccessfulFileSharingException(String message) {
        super(message);
    }

    public UnsuccessfulFileSharingException(String message, Throwable cause) {
        super(message, cause);
    }

    public UnsuccessfulFileSharingException(Throwable cause) {
        super(cause);
    }

    public UnsuccessfulFileSharingException(String message, Throwable cause, boolean enableSuppression, boolean writableStackTrace) {
        super(message, cause, enableSuppression, writableStackTrace);
    }
}
