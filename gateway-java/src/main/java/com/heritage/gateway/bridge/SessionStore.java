package com.heritage.gateway.bridge;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.time.Duration;
import java.time.Instant;
import java.util.HexFormat;
import java.util.Iterator;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

/**
 * In-memory session table plus HMAC-signed cookie helpers.
 *
 * <p>Cookie format: {@code <id>.<hex-hmac>}. The HMAC key is generated
 * at startup, so a restart logs everyone out. That's deliberate for now;
 * if persistence ever matters, swap in Redis without touching the
 * controller surface.
 */
@Component
public class SessionStore {

    private static final Logger log = LoggerFactory.getLogger(SessionStore.class);
    private static final SecureRandom RNG = new SecureRandom();

    private final byte[] hmacKey;
    private final Duration idleTimeout;
    private final Map<String, BankSession> entries = new ConcurrentHashMap<>();

    public SessionStore(@Value("${session.idle-timeout:PT30M}") Duration idleTimeout) {
        this.idleTimeout = idleTimeout;
        this.hmacKey = new byte[32];
        RNG.nextBytes(hmacKey);
    }

    public BankSession issue(String role, String username, BankConnection conn) {
        byte[] raw = new byte[24];
        RNG.nextBytes(raw);
        String id = HexFormat.of().formatHex(raw);
        BankSession s = new BankSession(id, role, username, conn);
        entries.put(id, s);
        return s;
    }

    public BankSession get(String id) {
        if (id == null) return null;
        BankSession s = entries.get(id);
        if (s == null) return null;
        if (Duration.between(s.lastUsedAt(), Instant.now()).compareTo(idleTimeout) > 0) {
            drop(id);
            return null;
        }
        s.touch();
        return s;
    }

    public void drop(String id) {
        BankSession s = entries.remove(id);
        if (s != null) {
            s.connection().close();
        }
    }

    public String sign(String id) {
        return id + "." + HexFormat.of().formatHex(hmac(id));
    }

    public String verify(String cookieValue) {
        if (cookieValue == null) return null;
        int dot = cookieValue.indexOf('.');
        if (dot <= 0 || dot == cookieValue.length() - 1) return null;
        String id = cookieValue.substring(0, dot);
        String sig = cookieValue.substring(dot + 1);
        byte[] want = hmac(id);
        byte[] got;
        try {
            got = HexFormat.of().parseHex(sig);
        } catch (IllegalArgumentException e) {
            return null;
        }
        return MessageDigest.isEqual(want, got) ? id : null;
    }

    @Scheduled(fixedDelay = 60_000)
    void sweep() {
        Instant now = Instant.now();
        Iterator<Map.Entry<String, BankSession>> it = entries.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, BankSession> e = it.next();
            if (Duration.between(e.getValue().lastUsedAt(), now).compareTo(idleTimeout) > 0) {
                log.info("evicting idle session for {}", e.getValue().username());
                e.getValue().connection().close();
                it.remove();
            }
        }
    }

    private byte[] hmac(String id) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(hmacKey, "HmacSHA256"));
            return mac.doFinal(Objects.requireNonNull(id).getBytes(StandardCharsets.UTF_8));
        } catch (Exception e) {
            throw new IllegalStateException("HMAC init failed", e);
        }
    }
}
