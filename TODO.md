# TODO

Punch list of follow-ups in rough priority order. See
[`SECURITY.md`](SECURITY.md) for the security-specific subset and
[`gateway-java/README.md`](gateway-java/README.md) for the
endpoint-implementation status table.

## High-impact, one-branch-each

- [ ] **Password hashing on disk.** `crypt_r($6$…)` plus a one-shot
      migration tool (`bank_server/src/migrate_hash.c`). Touches every
      `validate*` function and the change-password flows. Requires
      enlarging `MAX_PASS` (struct-layout change).
- [ ] **TLS on the gateway↔browser hop** via standard Spring
      `server.ssl.*` properties. Add a `make certs` target that mints
      a self-signed cert under `bank_server/certs/`.
- [ ] **TLS on the gateway↔bank_server hop**, or switch that hop to
      a Unix-domain socket. The bank_server-side change is the bigger
      part; OpenSSL wrap on top of the existing `read`/`write`.
- [ ] **`@SpringBootTest` + `MockBankServer` fixture.** Each
      `BankClient` method exercised against a Java `ServerSocket`
      that emits the documented prompt sequence. Today's safety net
      is the curl walk in `README.md`.
- [ ] **WebSocket push.** `/ws` channel for live balance / transaction
      updates. Gateway side: subscribe each session; emit when the
      next read after an operation lands. UI side: replace the
      explicit refresh in `CustomerDashboard`.

## Medium

- [ ] **`/api/customer/loans`** — let the customer dashboard list its
      own applications + statuses.
- [ ] **CSV/JSON migration of the binary data format** so schema
      changes don't break every existing file. Use a versioned header.
- [ ] **CSRF token** if/when the SPA and gateway split hosts.
- [ ] **Persistent session store (Redis)** so gateway restarts don't
      log everyone out.
- [ ] **`lock_utils.[ch]` refactor** to consolidate the duplicated
      `fcntl` blocks in the four `*_utils.c` files.
- [ ] **Central logging module** (`log.[ch]`) replacing the ad-hoc
      `perror` / `dprintf` calls.
- [ ] **Failed-login backoff + lockout** with an on-disk fail counter.
- [ ] **Pagination on the customer transaction table.** Currently
      renders all rows.
- [ ] **Balance sparkline / monthly chart** on the customer dashboard
      (Recharts).

## Low / cosmetic

- [ ] **Loan workflow business logic** — interest, repayment schedule,
      concurrent-loan limit, creditworthiness check.
- [ ] **Feedback state machine** (NEW → ACK → RESOLVED).
- [ ] **CI pipeline** verifying `make`, `mvn package`, `pnpm build` on
      each PR.
- [ ] **`MenuChoice` enum** in the bridge so `c.send("9")` reads as
      `c.send(CustomerMenu.LOGOUT)`.
- [ ] **Drop the standalone `init_sessions` binary** once everyone is
      on the auto-init server build. Keep it on disk for one release
      as a fallback.
- [ ] **`employee` console**: customer search + modify-customer
      dialog, transaction viewer for a specific customer.
- [ ] **Customer settings page** for password change as its own
      destination (currently a button in the at-a-glance card).
