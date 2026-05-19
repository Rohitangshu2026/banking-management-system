package com.heritage.gateway.controller;

import com.heritage.gateway.bridge.*;
import com.heritage.gateway.dto.LoginRequest;
import com.heritage.gateway.dto.MeDto;
import com.heritage.gateway.web.SessionGuard;
import jakarta.servlet.http.Cookie;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import jakarta.validation.Valid;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

@RestController
@RequestMapping("/api")
public class AuthController {

    private final BankClient bank;
    private final SessionStore store;
    private final SessionGuard guard;

    public AuthController(BankClient bank, SessionStore store, SessionGuard guard) {
        this.bank = bank;
        this.store = store;
        this.guard = guard;
    }

    @GetMapping("/health")
    public Map<String, Object> health() {
        // Real readiness probe: open a throwaway TCP connection to
        // bank_server and verify it sends the role-select menu within
        // a short window. A bare {status: ok} doesn't catch the case
        // where the gateway is up but the C server is dead.
        try (BankConnection probe = bank.dial()) {
            probe.readUntil(com.heritage.gateway.bridge.PromptMatcher.MAIN_MENU_CHOICE_PROMPT,
                    java.time.Duration.ofSeconds(2));
            return Map.of("status", "ok", "bank", "reachable");
        } catch (RuntimeException e) {
            return Map.of("status", "degraded", "bank", "unreachable",
                    "detail", e.getMessage());
        }
    }

    @PostMapping("/auth/login")
    public MeDto login(@Valid @RequestBody LoginRequest req,
                       HttpServletRequest httpReq, HttpServletResponse httpRes) {
        BankConnection conn = bank.dial();
        Integer accountId;
        try {
            accountId = bank.authenticate(conn, req.role(), req.username(), req.password());
        } catch (RuntimeException e) {
            conn.close();
            throw e;
        }
        BankSession session = store.issue(req.role(), req.username(), conn);
        if (accountId != null) {
            session.setAccountId(accountId);
        }

        setSessionCookie(httpReq, httpRes, store.sign(session.id()));
        return new MeDto(session.role(), session.username(), session.accountId());
    }

    @PostMapping("/auth/logout")
    public Map<String, String> logout(HttpServletRequest req, HttpServletResponse res) {
        BankSession s;
        try {
            s = guard.require(req);
        } catch (BankProtocolException e) {
            clearCookie(res);
            return Map.of("status", "ok");
        }
        int logoutChoice = switch (s.role()) {
            case "customer" -> 9;
            case "employee" -> 7;
            case "manager"  -> 5;
            case "admin"    -> 6;
            default -> 9;
        };
        synchronized (s.connection().lock()) {
            bank.logout(s, logoutChoice);
        }
        store.drop(s.id());
        clearCookie(res);
        return Map.of("status", "ok");
    }

    @GetMapping("/me")
    public MeDto me(HttpServletRequest req) {
        BankSession s = guard.require(req);
        return new MeDto(s.role(), s.username(), s.accountId());
    }

    private void setSessionCookie(HttpServletRequest req, HttpServletResponse res, String value) {
        Cookie c = new Cookie(SessionGuard.COOKIE_NAME, value);
        c.setHttpOnly(true);
        c.setPath("/");
        c.setSecure(req.isSecure());
        c.setAttribute("SameSite", "Strict");
        res.addCookie(c);
    }

    private void clearCookie(HttpServletResponse res) {
        Cookie c = new Cookie(SessionGuard.COOKIE_NAME, "");
        c.setPath("/");
        c.setMaxAge(0);
        c.setHttpOnly(true);
        res.addCookie(c);
    }
}
