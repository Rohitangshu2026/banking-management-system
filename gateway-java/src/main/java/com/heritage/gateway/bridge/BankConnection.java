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

    /**
     * Bytes received from bank_server that came AFTER the last matched
     * prompt. Without this, a single TCP read that delivers (e.g.)
     * "Login successful!\n===== CUSTOMER MENU =====\n...\nEnter your choice:"
     * would be returned in full from the first readUntil that matches
     * "===== CUSTOMER MENU =====", and the trailing "Enter your choice:"
     * would be silently dropped. The next readUntil would then deadlock
     * waiting for a prompt the C server has already sent.
     */
    private final StringBuilder residual = new StringBuilder();

    public BankConnection(String host, int port, int connectTimeoutMs) throws IOException {
        log.info("BankConnection: connecting to {}:{} (connectTimeout={}ms)", host, port, connectTimeoutMs);
        this.socket = new Socket();
        this.socket.connect(new InetSocketAddress(host, port), connectTimeoutMs);
        // Set a default read timeout so the input stream is created with a
        // sensible timeout already in effect, then individual reads
        // override it via setSoTimeout.
        this.socket.setSoTimeout(2000);
        this.in = socket.getInputStream();
        this.out = socket.getOutputStream();
        log.info("BankConnection: connected to {}:{}, localPort={}",
                host, port, socket.getLocalPort());
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

        // 1. Fast path: check the residual buffer from previous reads first.
        //    If a prompt is already present there, consume up to the end of
        //    the line containing it and leave the rest for the next call.
        if (residual.length() > 0) {
            String match = tryMatch(residual, prompts);
            if (match != null) {
                log.debug("readUntil: matched in residual ({} bytes consumed, {} remaining)",
                        match.length(), residual.length());
                return match;
            }
        }

        // 2. Slow path: read from the socket, seeded with whatever's left in
        //    residual, until a prompt matches or the deadline expires.
        long deadline = System.nanoTime() + timeout.toNanos();
        StringBuilder buf = new StringBuilder(residual);
        residual.setLength(0);
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
                log.debug("readUntil: blocking read, remainingMs={}", remainingMs);
                int n = in.read(chunk);
                if (n < 0) {
                    throw new BankProtocolException(502,
                            "bank_server closed the connection mid-protocol (got: "
                                    + truncate(buf.toString()) + ")");
                }
                log.debug("readUntil: got {} bytes", n);
                buf.append(new String(chunk, 0, n, StandardCharsets.UTF_8));
                String match = tryMatch(buf, prompts);
                if (match != null) {
                    return match;
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

    /**
     * If any of {@code prompts} appears in {@code src}, return everything up to
     * (and including) the newline that terminates the line containing the
     * prompt — or, if no newline follows the prompt, the whole accumulated
     * text. Anything after the boundary is moved into {@link #residual}
     * for the next call. Returns null if no prompt matches.
     *
     * <p>The line-aware boundary matters: callers like {@code customerBalance}
     * pattern-match on the line "Your current balance is: $X.XX\n", so the
     * dollar amount must come back in the returned string, not be stashed in
     * residual.
     */
    private String tryMatch(StringBuilder src, List<String> prompts) {
        String s = src.toString();
        for (String p : prompts) {
            int idx = s.indexOf(p);
            if (idx < 0) continue;
            int nl = s.indexOf('\n', idx + p.length());
            int end = (nl < 0) ? s.length() : nl + 1;
            String consumed = s.substring(0, end);
            // Move any bytes after `end` into residual so the next readUntil
            // sees them. If src IS residual, this also rewrites src in place.
            String leftover = (end < s.length()) ? s.substring(end) : "";
            if (src == residual) {
                residual.setLength(0);
                residual.append(leftover);
            } else {
                src.setLength(0);
                residual.setLength(0);
                residual.append(leftover);
            }
            return consumed;
        }
        return null;
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
