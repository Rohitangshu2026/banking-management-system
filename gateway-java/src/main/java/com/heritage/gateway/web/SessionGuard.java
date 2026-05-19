package com.heritage.gateway.web;

import com.heritage.gateway.bridge.BankProtocolException;
import com.heritage.gateway.bridge.BankSession;
import com.heritage.gateway.bridge.SessionStore;
import jakarta.servlet.http.Cookie;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.stereotype.Component;

/**
 * Resolves the {@code bms_session} cookie to a live {@link BankSession}.
 * Centralised so every controller method does the same thing.
 */
@Component
public class SessionGuard {

    public static final String COOKIE_NAME = "bms_session";

    private final SessionStore store;

    public SessionGuard(SessionStore store) {
        this.store = store;
    }

    public BankSession require(HttpServletRequest req) {
        Cookie[] cookies = req.getCookies();
        if (cookies == null) {
            throw new BankProtocolException(401, "not logged in");
        }
        for (Cookie c : cookies) {
            if (COOKIE_NAME.equals(c.getName())) {
                String id = store.verify(c.getValue());
                if (id == null) {
                    throw new BankProtocolException(401, "invalid session");
                }
                BankSession s = store.get(id);
                if (s == null) {
                    throw new BankProtocolException(401, "session expired");
                }
                return s;
            }
        }
        throw new BankProtocolException(401, "not logged in");
    }

    public BankSession requireRole(HttpServletRequest req, String role) {
        BankSession s = require(req);
        if (!role.equals(s.role())) {
            throw new BankProtocolException(403,
                    "this endpoint requires role=" + role + ", session role=" + s.role());
        }
        return s;
    }
}
