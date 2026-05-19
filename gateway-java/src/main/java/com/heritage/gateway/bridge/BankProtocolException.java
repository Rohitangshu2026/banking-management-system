package com.heritage.gateway.bridge;

/**
 * Thrown when the upstream bank_server returns a recognised failure
 * message, the TCP connection dies, or a {@code readUntil} times out.
 *
 * <p>The {@link #status} is the HTTP status the {@code GlobalExceptionHandler}
 * should map this to. Use {@code 422} for "user did something invalid",
 * {@code 502} for "bank_server is broken", {@code 504} for timeouts.
 */
public class BankProtocolException extends RuntimeException {
    private final int status;

    public BankProtocolException(int status, String message) {
        super(message);
        this.status = status;
    }

    public BankProtocolException(int status, String message, Throwable cause) {
        super(message, cause);
        this.status = status;
    }

    public int status() {
        return status;
    }
}
