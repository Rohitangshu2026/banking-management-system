# bms-gateway (Spring Boot)

Spring Boot 3.3 / Java 17 façade in front of `bank_server`. Replaces the
earlier Go gateway under `gateway/`. Same JSON surface, same cookie
scheme — `web/` is unchanged from its perspective.

## Why this exists

`bank_server` speaks a line-oriented, menu-driven TCP protocol that's
fine for the curses client but hostile to browsers: every request would
have to renegotiate the menu state machine.

The gateway holds one TCP connection per logged-in browser session,
serialises the menu walk for each HTTP request, and exposes a stable
JSON shape under `/api`.

```
Browser (React) ── HTTPS+cookie ──▶ Spring Boot gateway ── TCP ──▶ bank_server (C)
```

## Layout

```
src/main/java/com/heritage/gateway/
├── GatewayApplication.java        @SpringBootApplication entrypoint
├── config/BankProperties.java     bank.host/port/timeouts (env-driven)
├── bridge/
│   ├── BankConnection.java        TCP socket + readUntil/send primitives
│   ├── BankSession.java           per-user session metadata + lock holder
│   ├── SessionStore.java          ConcurrentHashMap + HMAC-signed cookies
│   ├── PromptMatcher.java         every literal prompt copy-pasted from
│   │                              bank_server's source, single point of truth
│   ├── BankProtocolException.java checked exception with HTTP status code
│   └── BankClient.java            high-level menu walks (one method per
│                                  bank_server menu option)
├── controller/
│   ├── AuthController.java        /api/health, /api/auth/login,logout,
│   │                              /api/me
│   ├── CustomerController.java    8 customer endpoints
│   ├── EmployeeController.java    5 employee endpoints
│   ├── ManagerController.java     4 manager endpoints
│   └── AdminController.java       4 admin endpoints + /api/logs
├── dto/                            request/response records, one per file
└── web/
    ├── SessionGuard.java          cookie → BankSession resolver + role gate
    ├── GlobalExceptionHandler.java BankProtocolException → JSON error
    └── CorsConfig.java            allow Vite dev server origins
```

## Run

```sh
cd gateway-java
mvn spring-boot:run        # dev — auto-restart on changes
```

```sh
mvn package
java -jar target/bms-gateway.jar
```

Both default to `http://localhost:8443`. The bank server must be
reachable at `BANK_HOST:BANK_PORT` (default `127.0.0.1:8080`).

## Configuration

`application.yml` defaults:

```yaml
server.port: 8443
bank:
  host: 127.0.0.1
  port: 8080
  connect-timeout-ms: 5000
  read-timeout-ms: 8000
session:
  idle-timeout: PT30M
```

All keys are env-overridable via Spring's relaxed binding —
`BANK_HOST`, `BANK_PORT`, `SERVER_PORT`, `SESSION_IDLE_TIMEOUT`.

TLS in production: set `SERVER_SSL_KEY_STORE`, `SERVER_SSL_KEY_STORE_PASSWORD`,
etc. via env. The cert path is intentionally not committed.

## API surface

| Method | Path | Role |
|---|---|---|
| GET | `/api/health` | — |
| POST | `/api/auth/login` | any |
| POST | `/api/auth/logout` | any |
| GET | `/api/me` | any |
| GET | `/api/account` | customer |
| GET | `/api/transactions` | customer |
| POST | `/api/deposit` | customer |
| POST | `/api/withdraw` | customer |
| POST | `/api/transfer` | customer |
| POST | `/api/loans` | customer |
| POST | `/api/feedback` | customer |
| POST | `/api/password` | any |
| POST | `/api/customers` | employee |
| PUT | `/api/customers/{username}` | employee |
| GET | `/api/customers/{username}/transactions` | employee |
| GET | `/api/loans/assigned` | employee |
| POST | `/api/loans/{id}/process` | employee |
| GET | `/api/loans/pending` | manager |
| POST | `/api/loans/{id}/assign` | manager |
| POST | `/api/customers/{username}/toggle` | manager |
| GET | `/api/feedback` | manager |
| POST | `/api/users` | admin |
| PUT | `/api/users/{username}` | admin |
| PATCH | `/api/users/{username}/role` | admin |
| GET | `/api/logs` | admin |

## Bridge mechanics

`BankClient` is where the bank_server protocol meets the JSON surface.
Every method follows the same shape:

```java
public BalanceDto customerDeposit(BankSession s, double amount) {
    BankConnection c = s.connection();
    c.send("2");                                              // menu choice
    c.readUntil(DEPOSIT_PROMPT, STD_TIMEOUT);                 // wait for prompt
    c.send(formatAmount(amount));                              // send input
    String resp = c.readUntil(List.of(DEPOSIT_OK_PREFIX, ...), STD_TIMEOUT);
    // pattern-match success or fail, parse, return DTO, return to menu
}
```

Three rules to follow when adding methods:

1. **Hold `session.connection().lock()`** for the whole walk; controllers do
   this with a `synchronized` block.
2. **Always read back to `MAIN_MENU_CHOICE_PROMPT` before returning** so the
   next HTTP call finds the socket parked at the menu prompt.
3. **Catch every failure pattern the C server can emit** for this menu —
   look at `bank_server/src/*_utils.c` for the `write()` lines — and map
   each to a `BankProtocolException(status, message)`.

Prompt strings live exclusively in `PromptMatcher`. If you find yourself
typing a literal in another file, move it there.

## Known limitations

- No persistent session store: gateway restart logs everyone out.
- No CSRF token; relies on `SameSite=Strict` cookie. Adequate same-origin,
  not enough cross-origin.
- No rate limiter (the previous Go gateway had a coarse per-IP one).
  Easy follow-up: `bucket4j` + `OncePerRequestFilter`.
- TLS on the internal hop to `bank_server` is plain TCP. Both should sit
  inside one trust boundary in production.
- No `WebSocket` push channel yet — every dashboard view polls.

## Tests

There are no `@SpringBootTest` cases in this revision. The audit-grade
verification is the curl smoke test at the bottom of the root README.
Mock-server tests should come next; bridge code is structured so each
`BankClient` method can be exercised against a `MockBankServer` that
emits the documented prompt sequence.
