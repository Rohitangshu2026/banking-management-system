package com.heritage.gateway.bridge;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.List;

/**
 * Thin wrapper around the TCP socket to bank_server. Owns the I/O streams
 * and the per-connection serialisation lock; controllers must hold the
 * lock for the duration of a menu walk.
 *
 * <p>Read semantics: {@link #readUntil(List, Duration)} accumulates bytes
 * into a buffer until any of the listed substrings appears or the read
 * deadline is reached. Returns the full accumulated text so callers can
 * pattern-match success/failure with a single helper.
 */
public class BankConnection implements AutoCloseable {

    private static final Logger log = LoggerFactory.getLogger(BankConnection.class);

    private final Socket socket;
    private final InputStream in;
    private final OutputStream out;
    private final Object lock = new Object();

    public BankConnection(String host, int port, int connectTimeoutMs) throws IOException {
        this.socket = new Socket();
        this.socket.connect(new InetSocketAddress(host, port), connectTimeoutMs);
        this.in = socket.getInputStream();
        this.out = socket.getOutputStream();
    }

    public Object lock() { return lock; }

    /**
     * Read bytes until any prompt substring appears in the buffer or the
     * timeout fires. Returns the full accumulated text.
     */
    public String readUntil(List<String> prompts, Duration timeout) {
        if (prompts == null || prompts.isEmpty()) {
            throw new IllegalArgumentException("prompts must not be empty");
        }
        long deadline = System.nanoTime() + timeout.toNanos();
        StringBuilder buf = new StringBuilder(512);
        byte[] chunk = new byte[512];

        try {
            while (true) {
                long remainingMs = Math.max(1, (deadline - System.nanoTime()) / 1_000_000);
                if (remainingMs <= 0) {
                    throw new BankProtocolException(504,
                            "timed out waiting for one of " + prompts + " (got: "
                                    + truncate(buf.toString()) + ")");
                }
                socket.setSoTimeout((int) Math.min(remainingMs, Integer.MAX_VALUE));
                int n = in.read(chunk);
                if (n < 0) {
                    throw new BankProtocolException(502,
                            "bank_server closed the connection mid-protocol (got: "
                                    + truncate(buf.toString()) + ")");
                }
                buf.append(new String(chunk, 0, n, StandardCharsets.UTF_8));
                String s = buf.toString();
                for (String p : prompts) {
                    if (s.contains(p)) return s;
                }
            }
        } catch (java.net.SocketTimeoutException e) {
            throw new BankProtocolException(504,
                    "timed out waiting for one of " + prompts + " (got: "
                            + truncate(buf.toString()) + ")", e);
        } catch (IOException e) {
            throw new BankProtocolException(502, "I/O error talking to bank_server", e);
        }
    }

    public String readUntil(String prompt, Duration timeout) {
        return readUntil(List.of(prompt), timeout);
    }

    /**
     * Send {@code line} terminated by a single newline (matching what the C
     * server's {@code readLine} expects).
     */
    public void send(String line) {
        try {
            byte[] bytes = (line + "\n").getBytes(StandardCharsets.UTF_8);
            out.write(bytes);
            out.flush();
        } catch (IOException e) {
            throw new BankProtocolException(502, "I/O error writing to bank_server", e);
        }
    }

    /** Drains anything currently readable without blocking; best-effort. */
    public void drain() {
        try {
            socket.setSoTimeout(50);
            byte[] tmp = new byte[256];
            while (in.available() > 0 && in.read(tmp) > 0) {
                // discard
            }
        } catch (IOException ignored) {
            // best-effort
        }
    }

    @Override
    public void close() {
        try {
            socket.close();
        } catch (IOException e) {
            log.debug("error closing socket", e);
        }
    }

    private static String truncate(String s) {
        String trimmed = s.replace("\n", "\\n");
        return trimmed.length() > 200 ? trimmed.substring(trimmed.length() - 200) : trimmed;
    }
}
