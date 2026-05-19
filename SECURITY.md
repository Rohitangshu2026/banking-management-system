# Security posture

What this branch defends against, what it doesn't, and the threat model
to keep in mind.

## In place

| Control | Where | Notes |
| ------- | ----- | ----- |
| Strict integer parsing on menu input | `bank_server/src/common.c` (`parse_int_strict`) | `atoi` previously treated `"asdf"` as `0`. |
| Bounded string copies | `safe_strcpy` in `common.c`, used by `employee_utils.c` and the loan-status write | Replaced the unsafe `strcpy` sites. |
| `0600` data file permissions | every `O_CREAT` in the C server | World-readable `0644` was the previous default. |
| `fsync` on critical writes + rollback on partial-write failure | balance, user, session, loan, feedback flows | |
| Per-record `fcntl` locks | every `*_utils.c` mutation path | Transfers acquire the two account locks in ascending-id order to avoid deadlock. |
| Compiler hardening | `bank_server/Makefile` | `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-fPIE`; Linux gets RELRO + BIND_NOW too. |
| HMAC-signed session cookies | `gateway-java/.../SessionStore` | 32-byte random key, regenerated each gateway boot. |
| Per-IP rate limit on `/api/*` | `RateLimitFilter` | 120 req/min, returns `429` with `Retry-After: 60`. |
| Strict role gating | `SessionGuard.requireRole` | Mismatched role → `403`, never silently coerced. |
| TOCTOU close in customer login | `validateCustomer` rechecks `isActive` after acquiring the session lock | |
| Auto-id overflow guard | `LOAN_ID_BASE` / `CUSTOMER_ID_BASE` / `MAX_AUTO_ID` in `common.h` | Hard cap leaves headroom before `INT_MAX` wraps. |
| `SameSite=Strict; HttpOnly` cookies | `AuthController` | Adequate same-origin; not a substitute for CSRF tokens if SPA and gateway split hosts. |
| Spring `BankProtocolException → JSON` | `GlobalExceptionHandler` | Internal stack traces never leak to clients. |

## Not in place — known follow-ups

| Gap | Severity | Why deferred |
| --- | -------- | ------------ |
| **Plaintext passwords on disk** | Critical | Requires `crypt_r($6$…)` migration, larger `MAX_PASS` (struct-layout change), and a one-shot migrator binary. Out of scope for this branch because it breaks every existing data file at once. |
| **TLS on the bank_server hop** | High | The gateway talks plain TCP to bank_server. Acceptable when both live on a single host inside one trust boundary; not acceptable across a network. Cleanest fix is a Unix-domain socket. |
| **TLS on the gateway↔browser hop** | High | Disabled by default. Configurable via the standard Spring `server.ssl.*` properties once a cert is provisioned. |
| **CSRF tokens** | Medium | Currently relying on `SameSite=Strict`. Required the moment the SPA is served from a different origin than the gateway. |
| **No `@SpringBootTest` coverage** | Medium | Smoke test is `curl`. A `MockBankServer` fixture is the obvious next step. |
| **Failed-login backoff / lockout** | Medium | No on-disk fail counter. Brute force on the bank_server menu is currently unrestricted. |
| **Session timeout sweep is in-memory** | Low | Gateway restart drops every session. Adequate at demo scale; needs Redis for HA. |
| **Magic constants for menu choices** | Low | The bridge has e.g. `c.send("9")` for customer logout. A `MenuChoice` enum would document intent. |
| **Internal logging** | Low | `bank_server` still uses ad-hoc `perror` / `dprintf`; a single `log_*` module would centralise. |

## Threat model

Assumed adversary: a malicious **authenticated** customer trying to
read other customers' data, escalate to manager/admin, or corrupt the
shared data files.

Assumed *not in scope*: hostile traffic from an unauthenticated network
attacker. The gateway is intended to sit on a private/localhost network
behind a TLS-terminating proxy. The "no TLS, no CSRF token" stance
makes this explicit.

If you're deploying this for real, do at minimum:

1. Front the gateway with nginx/Caddy doing real TLS termination.
2. Run `bank_server` and the gateway on a Unix-domain socket on the same
   host; remove the `127.0.0.1:8080` listener.
3. Hash the on-disk passwords (the largest remaining item).
4. Add CSRF tokens if the SPA isn't same-origin.
