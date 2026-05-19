# Banking Management System

A multi-role banking system written in C, with a fork-per-client TCP
server, a CLI client, an HTTP/JSON gateway, and a React web console.

> Originally a System Software course project (CSE513A). This branch is
> a hardening pass: it tightens the C server, adds a Spring Boot gateway in front
> of it so a browser can speak to the bank, and ships a React UI with
> a classic-bank visual language.

---

## Architecture

```
┌──────────┐  HTTPS + cookie  ┌──────────────┐    TCP    ┌──────────────┐
│ Browser  │ ───────────────► │ Spring Boot gateway   │ ────────► │ bank_server  │
│  (React) │                  │ (HTTP / WS)  │           │  (C, fcntl)  │
└──────────┘                  └──────────────┘           └──────────────┘
                                                                │
                                            fcntl record locks  │
                                                                ▼
                                                 ┌──────────────────────┐
                                                 │ bank_server/data/*.txt│
                                                 │  binary-struct files  │
                                                 └──────────────────────┘
```

Two access paths are supported in parallel:

- **CLI client** — `client.c`, talks the line-oriented protocol over
  plain TCP. The original interface; still works.
- **Web** — React SPA in `web/`, calls `/api/*` on the gateway, which
  holds one TCP connection per logged-in browser session and walks the
  bank server's menu state machine on the user's behalf.

The gateway exists because asking a browser to renegotiate a
menu-driven TCP protocol on every request is a bad time. The bank
server is unchanged from the CLI's perspective.

## Roles

Each role has its own menu in the bank server and its own console in
the web app.

| Role     | Can do                                                                                       |
| -------- | -------------------------------------------------------------------------------------------- |
| Customer | View balance, deposit, withdraw, transfer, apply for a loan, leave feedback, change password |
| Employee | Open customer accounts, modify customer details, approve / reject assigned loans              |
| Manager  | Activate / deactivate customer accounts, assign loans to employees, review feedback           |
| Admin    | Manage employees and managers, change roles, audit the activity log                           |

## Repository layout

```
.
├── bank_server/         # The C server
│   ├── include/         # Public headers
│   ├── src/             # Implementation
│   ├── data/            # Binary-struct data files (users, customers, …)
│   └── Makefile
├── client.c             # CLI client (talks plain TCP to bank_server)
├── gateway-java/        # Spring Boot 3.3 / Java 17 gateway
│   ├── pom.xml          # Maven build (Spring Boot starter parent)
│   ├── src/main/java/com/heritage/gateway/
│   │   ├── bridge/      # BankConnection, BankClient, PromptMatcher, …
│   │   ├── controller/  # Auth / Customer / Employee / Manager / Admin
│   │   ├── dto/         # request + response records
│   │   ├── web/         # SessionGuard, CORS, exception handler
│   │   └── config/      # BankProperties
│   ├── src/main/resources/application.yml
│   └── README.md
└── web/                 # Vite + React + TS frontend
    ├── src/pages/       # Login + four role consoles
    ├── src/lib/         # api.ts, format.ts
    ├── tailwind.config.ts
    └── README.md
```

## Quick start

You need a C toolchain, JDK 17, Maven 3.9+, Node + pnpm (or npm), and
a Unix machine. Tested on macOS (Darwin 25) with clang and OpenJDK 17,
and on Linux with gcc.

```sh
# 1. Build & run the bank server
cd bank_server
make all              # builds server, init_sessions, ../client
./init_sessions       # one-time, seeds data/sessions.txt
./server              # listens on :8080

# 2. Build & run the gateway (in a second terminal)
cd gateway-java
mvn spring-boot:run   # listens on :8443

# 3. Run the web app (in a third terminal)
cd web
pnpm install
pnpm dev              # http://localhost:5173
```

Defaults assume everything is on `127.0.0.1`. Override via flags or env
vars — see [`gateway-java/README.md`](gateway-java/README.md) and
[`web/README.md`](web/README.md).

## Building

### bank_server

```sh
cd bank_server
make             # server only
make all         # server + init_sessions + ../client
make debug       # ASan + UBSan + -O0; for catching runtime bugs
make clean
```

CFLAGS default to `-O2 -g -Wall -Wextra -Wstrict-prototypes -Wshadow
-Wpointer-arith -Wcast-align -fstack-protector-strong -D_FORTIFY_SOURCE=2
-fPIE -MMD -MP`. On Linux, LDFLAGS adds `-pie -Wl,-z,relro,-z,now`.

The `.d` files emitted by `-MMD` give incremental builds when headers
change. Don't disable them unless you have a good reason.

### gateway-java

```sh
cd gateway-java
mvn package                    # produces target/bms-gateway.jar
java -jar target/bms-gateway.jar
```

Maven downloads Spring Boot and Jackson into the local cache on first
build. Subsequent builds are incremental.

### web

```sh
cd web
pnpm install
pnpm build       # emits dist/
```

## Data files

`bank_server/data/` holds the canonical state:

| File              | Records           | Purpose                            |
| ----------------- | ----------------- | ---------------------------------- |
| `users.txt`       | `User`            | All non-admin user records         |
| `admins.txt`      | `Admin`           | Admins only                        |
| `customers.txt`   | `Customer`        | Account balances                   |
| `sessions.txt`    | `Session`         | One slot per user; fcntl-locked    |
| `transactions.txt`| `Transaction`     | Append-only audit                  |
| `loans.txt`       | `Loan`            | Loan applications and their status |
| `feedback.txt`    | `Feedback`        | Customer feedback                  |
| `logs.txt`        | text              | Admin/manager action log           |

All record sizes are fixed (binary struct write). That keeps the
locking trivial (every record is one byte range) at the cost of making
schema evolution painful — adding a field breaks every existing file.
There's a follow-up to introduce a versioned record header before that
becomes a real problem.

Files are created with mode `0600`. Existing files from earlier upload
commits keep the mode they had; run `chmod 600 bank_server/data/*.txt`
once after first checkout.

## Concurrency model

- `fork()` per accepted connection. No threads, no event loop. Child
  closes the listening fd; parent closes the client fd.
- `fcntl(F_SETLK | F_SETLKW)` for per-record locking on every data
  file. Locks survive across `fork()` because they're tied to the
  inode, not the fd.
- Transfers acquire two account locks in ascending order of account
  id to avoid the classic two-account deadlock.
- SIGINT triggers a clean shutdown: `keepRunning` is set, `accept()`
  returns from `EINTR`, the parent closes the listener and waits for
  children to reap themselves via `SIGCHLD`.

## Security posture

What's done in this branch:

- Strict integer parsing on every menu choice (`parse_int_strict`).
  Prior `atoi()`-only parsing silently treated garbage as choice 0.
- Bounded `safe_strcpy` on the unsafe `strcpy` sites — most notably
  the caller-allocated-buffer copy in
  `employee_utils.c::getCustomerUsernameByAccountId`.
- Data files created `0600` rather than `0644`.
- `fsync()` on every balance / user / session / loan write, with
  rollback on partial failure.
- Locks held while transaction-log writes happen so balance + log are
  consistent from a reader's perspective.
- Compiler hardening: stack canaries, `_FORTIFY_SOURCE=2`, PIE,
  RELRO+BIND_NOW on Linux. Warning set tuned to catch shadowing,
  pointer arithmetic on `void*`, and unaligned casts.
- Gateway terminates TLS for the browser; supports HMAC-signed
  cookies and per-IP rate limiting that the C server lacks.

What's *not* done, by design or because it's bigger than one branch:

- **Passwords are still plaintext on disk.** The migration to
  `crypt_r($6$...)` requires enlarging `MAX_PASS` (a struct-layout
  change) and a one-shot migrator. Tracked separately.
- **TLS on the internal hop** between gateway and bank_server is not
  enabled. Both should sit behind a single trust boundary in
  production, ideally on a Unix-domain socket.
- **Rate limiting and CSRF** live in the gateway only; the C server
  trusts whatever connects to it on `:8080`.
- **WebSocket push** for live balance updates — endpoint surface
  exists, handler doesn't. See `gateway-java/README.md` punch list.

## Development tips

- `make debug` in `bank_server/` gives you an ASan build. Run that
  whenever you touch lock or write paths; the binary is several times
  slower but will catch heap and stack errors immediately.
- Two clients hammering the same pair of accounts is the easiest way
  to smoke-test the transfer locking. The balance sum should be
  invariant.
- The gateway only requires `bank_server` to be reachable on the TCP
  port at *login* time; once authenticated, the bridge holds the
  connection open. Restarting the bank server therefore drops every
  active web session — by design.

## Original project

The course-project history is preserved in the git log: the initial
upload commits land first; the hardening commits build on top of them.
The class diagram is at [`ClassDiagram.png`](ClassDiagram.png).

## License

Course project. No formal license declared yet; treat as
all-rights-reserved until one is added.
