package com.heritage.gateway.bridge;

import java.time.Instant;

/**
 * Per-user session state. Holds the live TCP connection to bank_server
 * (the user is currently parked in their role-specific menu) plus
 * identity metadata.
 *
 * <p>All bank_server interactions for this session must happen while
 * holding {@code connection.lock()}.
 */
public class BankSession {

    private final String id;
    private final String role;
    private final String username;
    private final BankConnection connection;
    /** Customer-only: the account id, captured at login when known. */
    private volatile Integer accountId;
    private volatile Instant lastUsedAt;

    public BankSession(String id, String role, String username, BankConnection connection) {
        this.id = id;
        this.role = role;
        this.username = username;
        this.connection = connection;
        this.lastUsedAt = Instant.now();
    }

    public String id()        { return id; }
    public String role()      { return role; }
    public String username()  { return username; }
    public BankConnection connection() { return connection; }
    public Integer accountId()         { return accountId; }
    public void setAccountId(Integer id) { this.accountId = id; }
    public Instant lastUsedAt()        { return lastUsedAt; }
    public void touch() { this.lastUsedAt = Instant.now(); }
}
