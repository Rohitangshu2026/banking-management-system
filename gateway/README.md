# bms-gateway

HTTP/JSON façade in front of `bank_server`. One Go binary, no
runtime dependencies beyond the standard library.

## Why this exists

`bank_server` speaks a line-oriented, menu-driven TCP protocol that is
fine for a curses client but very awkward to consume from a browser:
every request would have to renegotiate the menu state machine.

The gateway holds one TCP connection per logged-in browser session,
advances the menus on behalf of the user, and exposes a stable JSON
shape over HTTPS plus a WebSocket channel for push updates.

```
+---------+  HTTPS+cookie   +-----------+   TCP   +-------------+
| browser | <-------------> | gateway   | <-----> | bank_server |
+---------+    WebSocket    +-----------+         +-------------+
```

## Run

```sh
go build -o bank-gateway
./bank-gateway -bank 127.0.0.1:8080 -listen :8443 \
    -cert certs/server.crt -key certs/server.key
```

Without `-cert/-key` the gateway listens over plain HTTP — fine for
local development, never for production. The accompanying React build,
if present at `../web/dist`, is served at `/`.

Environment variable equivalents:

| Flag             | Env var               | Default            |
| ---------------- | --------------------- | ------------------ |
| `-listen`        | `GATEWAY_LISTEN`      | `:8443`            |
| `-bank`          | `BANK_ADDR`           | `127.0.0.1:8080`   |
| `-web`           | `GATEWAY_WEB_DIR`     | `../web/dist`      |
| `-cert`          | `GATEWAY_TLS_CERT`    | _empty (no TLS)_   |
| `-key`           | `GATEWAY_TLS_KEY`     | _empty (no TLS)_   |

## API surface

| Method | Path                          | Status        |
| ------ | ----------------------------- | ------------- |
| GET    | `/api/health`                 | ✅ implemented |
| POST   | `/api/auth/login`             | ✅ implemented |
| POST   | `/api/auth/logout`            | ✅ implemented |
| GET    | `/api/me`                     | ✅ implemented |
| GET    | `/api/transactions`           | ⏳ 501          |
| POST   | `/api/deposit`                | ⏳ 501          |
| POST   | `/api/withdraw`               | ⏳ 501          |
| POST   | `/api/transfer`               | ⏳ 501          |
| POST   | `/api/loans`                  | ⏳ 501          |
| POST   | `/api/feedback`               | ⏳ 501          |
| POST   | `/api/password`               | ⏳ 501          |
| POST   | `/api/customers`              | ⏳ 501          |
| GET    | `/api/customers/{id}`         | ⏳ 501          |
| POST   | `/api/loans/{id}/process`     | ⏳ 501          |
| POST   | `/api/customers/{id}/toggle`  | ⏳ 501          |
| POST   | `/api/loans/{id}/assign`      | ⏳ 501          |
| GET    | `/api/feedback`               | ⏳ 501          |
| GET    | `/api/users`                  | ⏳ 501          |
| PATCH  | `/api/users/{id}/role`        | ⏳ 501          |

## Adding an endpoint

The mechanics are the same for every call:

1. Add a route in `routes.go` that authenticates via `requireSession`.
2. Add a method on `*bankConn` in `bridge.go` that walks the
   bank_server menu (read prompt → send choice → read prompt → send
   data → read result). The existing `authenticate` method is the
   reference shape.
3. Marshal the response into a struct that `writeJSON` can encode.
4. Update the table above.

Two rules worth following:

- **Always hold `bankConn.mu` for the duration of a menu transition.**
  Two HTTP requests on the same session must not interleave bytes on
  the same TCP socket.
- **Validate at this boundary, not just on the React side.** The
  bank_server now does strict integer parsing, but the gateway should
  also reject empty / out-of-range amounts so the React app gets a
  4xx instead of a confusing error string from the menu.

## TLS

The gateway is the only thing in the system that should hold a
browser-facing certificate. The internal hop to `bank_server` is
plain TCP today; once TLS lands on the C side, point the gateway at
the internal cert via a separate flag — do not share the public cert
with the bank server.

## Known limitations

- No persistent session store; restart logs everyone out.
- No CSRF token — relies on `SameSite=Strict` cookies, which is
  adequate for a same-origin SPA but would need a token if we ever
  embed the gateway behind a different host.
- Rate limit is a simple per-minute bucket; a real deployment should
  use `x/time/rate` and key by `(ip, route)`.
