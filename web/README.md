# bms-web

The React + Vite frontend. Talks to the Go gateway under `/api`; the
gateway in turn proxies to `bank_server` over TCP.

## Stack

- Vite + React 18 + TypeScript
- Tailwind CSS for layout and the in-house palette
- React Router for client-side navigation
- Web Fonts: Fraunces (display), Inter (body), JetBrains Mono (numerics)

## Run locally

```sh
pnpm install         # or: npm install / yarn
pnpm dev             # http://localhost:5173
```

The dev server proxies `/api/*` to `http://127.0.0.1:8443` by default.
Override with `VITE_API_TARGET=...` if your gateway is elsewhere.

## Build

```sh
pnpm build
```

Emits `dist/`. The gateway can serve it directly:

```sh
../gateway/bank-gateway -web ./dist
```

## Design language

"Classic, elegant, rich." Restraint, hairline borders, generous
whitespace, paper-coloured surfaces. The palette lives in
`tailwind.config.ts`:

| Token            | Hex       | Use                                       |
| ---------------- | --------- | ----------------------------------------- |
| `ink`            | `#0B1220` | primary text, primary buttons             |
| `parchment`      | `#F7F3EC` | page background                           |
| `parchment-soft` | `#FBF8F2` | cards (`.plate`)                          |
| `gilded`         | `#B08D57` | accents only — section rules, focus rings |
| `slate2`         | `#5C6478` | metadata, labels                          |

Numerics (amounts, account ids, dates) always use the `.num` class so
columns line up with tabular figures.

## Layout

- `src/main.tsx` — entry point
- `src/App.tsx` — route table and role gating
- `src/auth.tsx` — auth context (calls `/api/me` on mount)
- `src/lib/api.ts` — single place that knows about `/api/*`
- `src/lib/format.ts` — money / account-id / date formatters
- `src/components/AppShell.tsx` — sidebar + header chrome
- `src/pages/` — Login, CustomerDashboard, EmployeeConsole,
  ManagerConsole, AdminConsole

## State of the UI

- **Login** — wired end-to-end against the gateway.
- **Customer dashboard** — fully styled, but populated with mock data
  until the gateway exposes `/api/transactions` and `/api/me` returns
  the balance shape. Swapping in real data is a one-line change in
  each section.
- **Employee / Manager / Admin** consoles — scaffolded with the nav
  and chrome; tables and dialogs are stubs.

See `../gateway/README.md` for the endpoint punch list that gates the
rest of the views.
