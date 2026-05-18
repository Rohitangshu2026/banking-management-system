import AppShell from "../components/AppShell";
import { fmtAccount, fmtDate, fmtMoney, fmtTime } from "../lib/format";

// Mock data — replaced once /api/transactions and /api/me return the
// account+balance shape. The structure here matches the eventual API,
// so wiring it up is a one-line swap in each section.
const account = {
  id: 1042,
  holder: "Aria Whitfield",
  balance: 184_320.55,
  available: 184_320.55,
  opened: "2019-03-14",
};

const txns = [
  {
    id: 4181,
    ts: "2026-05-18T14:22:00Z",
    type: "TRANSFER" as const,
    amount: -1450,
    counterparty: "0000001211",
    memo: "Rent — May",
  },
  {
    id: 4180,
    ts: "2026-05-17T09:05:00Z",
    type: "DEPOSIT" as const,
    amount: 7200,
    counterparty: null,
    memo: "Payroll, Atlas & Co.",
  },
  {
    id: 4179,
    ts: "2026-05-15T19:41:00Z",
    type: "WITHDRAW" as const,
    amount: -300,
    counterparty: null,
    memo: "ATM, 5th Ave",
  },
  {
    id: 4178,
    ts: "2026-05-13T11:13:00Z",
    type: "TRANSFER" as const,
    amount: 850,
    counterparty: "0000000914",
    memo: "From J. Vance",
  },
  {
    id: 4177,
    ts: "2026-05-11T08:55:00Z",
    type: "DEPOSIT" as const,
    amount: 240,
    counterparty: null,
    memo: "Refund, Helix Health",
  },
];

const nav = [
  { label: "Overview", to: "/customer" },
  { label: "Transactions", to: "/customer/transactions" },
  { label: "Transfers", to: "/customer/transfers" },
  { label: "Loans", to: "/customer/loans" },
  { label: "Statements", to: "/customer/statements" },
  { label: "Settings", to: "/customer/settings" },
];

export default function CustomerDashboard() {
  return (
    <AppShell navItems={nav} title="Overview">
      <section className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        <div className="lg:col-span-2 plate p-7">
          <div className="text-xs uppercase tracking-[0.14em] text-slate2">
            Primary checking
          </div>
          <div className="mt-1 flex items-baseline gap-3">
            <span className="num text-xs text-slate2">
              {fmtAccount(account.id)}
            </span>
            <span className="text-xs text-slate2">·</span>
            <span className="text-xs text-slate2">
              opened {fmtDate(account.opened)}
            </span>
          </div>

          <div className="mt-6 font-display text-5xl num text-ink">
            {fmtMoney(account.balance)}
          </div>
          <div className="mt-1 text-sm text-slate2">
            Available {fmtMoney(account.available)}
          </div>

          <div className="rule-gilded mt-7" />

          <div className="mt-6 flex flex-wrap gap-3">
            <button className="btn-primary">Transfer</button>
            <button className="btn-ghost">Deposit</button>
            <button className="btn-ghost">Withdraw</button>
            <button className="btn-ghost">Apply for loan</button>
          </div>
        </div>

        <div className="plate p-7">
          <div className="text-xs uppercase tracking-[0.14em] text-slate2">
            At a glance
          </div>
          <ul className="mt-5 space-y-4">
            <Stat label="Income, this month" value={fmtMoney(7440)} />
            <Stat label="Spend, this month" value={fmtMoney(1750)} tone="ink" />
            <Stat label="Open loans" value="None" tone="muted" />
            <Stat label="Feedback status" value="—" tone="muted" />
          </ul>
        </div>
      </section>

      <section className="mt-10">
        <div className="flex items-end justify-between">
          <h2 className="font-display text-2xl">Recent activity</h2>
          <a href="#" className="text-sm text-gilded-deep hover:underline">
            View all
          </a>
        </div>

        <div className="mt-4 plate overflow-hidden">
          <table className="w-full text-sm">
            <thead className="bg-ink/[0.025] text-left text-xs uppercase tracking-[0.1em] text-slate2">
              <tr>
                <th className="px-5 py-3 font-medium">Date</th>
                <th className="px-5 py-3 font-medium">Description</th>
                <th className="px-5 py-3 font-medium">Counterparty</th>
                <th className="px-5 py-3 font-medium text-right">Amount</th>
              </tr>
            </thead>
            <tbody>
              {txns.map((t, i) => (
                <tr
                  key={t.id}
                  className={i % 2 ? "bg-parchment/40" : "bg-transparent"}
                >
                  <td className="px-5 py-3 align-top">
                    <div>{fmtDate(t.ts)}</div>
                    <div className="text-xs text-slate2">{fmtTime(t.ts)}</div>
                  </td>
                  <td className="px-5 py-3 align-top">
                    <div className="font-medium">{labelFor(t.type)}</div>
                    <div className="text-xs text-slate2">{t.memo}</div>
                  </td>
                  <td className="px-5 py-3 align-top num text-ink/80">
                    {t.counterparty ?? "—"}
                  </td>
                  <td
                    className={`px-5 py-3 align-top text-right num ${
                      t.amount < 0 ? "text-ink" : "text-emerald-800"
                    }`}
                  >
                    {fmtMoney(t.amount)}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>
    </AppShell>
  );
}

function Stat({
  label,
  value,
  tone = "ink",
}: {
  label: string;
  value: string;
  tone?: "ink" | "muted";
}) {
  return (
    <li className="flex items-baseline justify-between">
      <span className="text-sm text-slate2">{label}</span>
      <span
        className={`num text-sm ${tone === "muted" ? "text-slate2" : "text-ink"}`}
      >
        {value}
      </span>
    </li>
  );
}

function labelFor(t: "DEPOSIT" | "WITHDRAW" | "TRANSFER") {
  switch (t) {
    case "DEPOSIT":
      return "Deposit";
    case "WITHDRAW":
      return "Withdrawal";
    case "TRANSFER":
      return "Transfer";
  }
}
