import { useCallback, useEffect, useMemo, useState } from "react";
import AppShell from "../components/AppShell";
import Dialog from "../components/Dialog";
import { useToast } from "../components/Toast";
import { api, ApiError, type Balance, type Transaction } from "../lib/api";
import { fmtAccount, fmtDate, fmtMoney, fmtTime } from "../lib/format";

const nav = [
  { label: "Overview", to: "/customer" },
  { label: "Transactions", to: "/customer/transactions" },
  { label: "Transfers", to: "/customer/transfers" },
  { label: "Loans", to: "/customer/loans" },
  { label: "Statements", to: "/customer/statements" },
  { label: "Settings", to: "/customer/settings" },
];

type DialogKind = null | "deposit" | "withdraw" | "transfer" | "loan" | "feedback" | "password";

export default function CustomerDashboard() {
  const toast = useToast();
  const [balance, setBalance] = useState<Balance | null>(null);
  const [txns, setTxns] = useState<Transaction[]>([]);
  const [loading, setLoading] = useState(true);
  const [dialog, setDialog] = useState<DialogKind>(null);

  const refresh = useCallback(async () => {
    try {
      const [b, t] = await Promise.all([api.account(), api.transactions()]);
      setBalance(b);
      setTxns(t);
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : "Could not load account data";
      toast.show("error", msg);
    } finally {
      setLoading(false);
    }
  }, [toast]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const summary = useMemo(() => {
    let income = 0;
    let spend = 0;
    for (const t of txns) {
      if (t.amount > 0) income += t.amount;
      else spend += -t.amount;
    }
    return { income, spend };
  }, [txns]);

  return (
    <AppShell navItems={nav} title="Overview">
      <section className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        <div className="lg:col-span-2 plate p-7">
          <div className="text-xs uppercase tracking-[0.14em] text-slate2">
            Primary checking
          </div>
          <div className="mt-1 flex items-baseline gap-3">
            <span className="num text-xs text-slate2">
              {balance ? fmtAccount(balance.accountId) : "—"}
            </span>
          </div>

          <div className="mt-6 font-display text-5xl num text-ink">
            {loading || !balance ? (
              <Skeleton width="14rem" height="3.5rem" />
            ) : (
              fmtMoney(balance.balance)
            )}
          </div>
          <div className="mt-1 text-sm text-slate2">
            {balance ? `Available ${fmtMoney(balance.balance)}` : ""}
          </div>

          <div className="rule-gilded mt-7" />

          <div className="mt-6 flex flex-wrap gap-3">
            <button className="btn-primary" onClick={() => setDialog("transfer")}>Transfer</button>
            <button className="btn-ghost" onClick={() => setDialog("deposit")}>Deposit</button>
            <button className="btn-ghost" onClick={() => setDialog("withdraw")}>Withdraw</button>
            <button className="btn-ghost" onClick={() => setDialog("loan")}>Apply for loan</button>
            <button className="btn-ghost" onClick={() => setDialog("feedback")}>Leave feedback</button>
          </div>
        </div>

        <div className="plate p-7">
          <div className="text-xs uppercase tracking-[0.14em] text-slate2">
            At a glance
          </div>
          <ul className="mt-5 space-y-4">
            <Stat label="Income, recent" value={fmtMoney(summary.income)} />
            <Stat label="Spend, recent" value={fmtMoney(summary.spend)} />
            <Stat
              label="Recent activity"
              value={`${txns.length} item${txns.length === 1 ? "" : "s"}`}
              tone="muted"
            />
          </ul>
          <div className="rule-gilded mt-6" />
          <button
            className="mt-5 btn-ghost w-full"
            onClick={() => setDialog("password")}
          >
            Change password
          </button>
        </div>
      </section>

      <section className="mt-10">
        <div className="flex items-end justify-between">
          <h2 className="font-display text-2xl">Recent activity</h2>
          <button
            onClick={() => void refresh()}
            className="text-sm text-gilded-deep hover:underline"
          >
            Refresh
          </button>
        </div>

        <div className="mt-4 plate overflow-hidden">
          {loading ? (
            <div className="p-6 space-y-3">
              {[0, 1, 2].map((i) => (
                <Skeleton key={i} width="100%" height="2.5rem" />
              ))}
            </div>
          ) : txns.length === 0 ? (
            <div className="px-6 py-10 text-center text-sm text-slate2">
              No transactions yet. Your future deposits and transfers will appear here.
            </div>
          ) : (
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
                    key={`${t.ts}-${i}`}
                    className={i % 2 ? "bg-parchment/40" : "bg-transparent"}
                  >
                    <td className="px-5 py-3 align-top">
                      <div>{fmtDate(t.ts)}</div>
                      <div className="text-xs text-slate2">{fmtTime(t.ts)}</div>
                    </td>
                    <td className="px-5 py-3 align-top">
                      <div className="font-medium">{labelFor(t.type)}</div>
                      <div className="text-xs text-slate2">{t.memo ?? "—"}</div>
                    </td>
                    <td className="px-5 py-3 align-top num text-ink/80">
                      {t.counterpartyAccount != null
                        ? fmtAccount(t.counterpartyAccount)
                        : "—"}
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
          )}
        </div>
      </section>

      <DepositDialog
        open={dialog === "deposit"}
        onClose={() => setDialog(null)}
        onDone={refresh}
      />
      <WithdrawDialog
        open={dialog === "withdraw"}
        onClose={() => setDialog(null)}
        onDone={refresh}
      />
      <TransferDialog
        open={dialog === "transfer"}
        onClose={() => setDialog(null)}
        onDone={refresh}
      />
      <LoanDialog open={dialog === "loan"} onClose={() => setDialog(null)} />
      <FeedbackDialog
        open={dialog === "feedback"}
        onClose={() => setDialog(null)}
      />
      <PasswordDialog
        open={dialog === "password"}
        onClose={() => setDialog(null)}
      />
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

function Skeleton({ width, height }: { width: string; height: string }) {
  return (
    <div
      className="rounded bg-ink/5 animate-pulse"
      style={{ width, height }}
    />
  );
}

function labelFor(t: "DEPOSIT" | "WITHDRAW" | "TRANSFER" | "LOAN") {
  switch (t) {
    case "DEPOSIT":  return "Deposit";
    case "WITHDRAW": return "Withdrawal";
    case "TRANSFER": return "Transfer";
    case "LOAN":     return "Loan disbursal";
  }
}

// ------------------------- Dialog components -------------------------

function AmountDialog({
  open,
  onClose,
  onDone,
  title,
  cta,
  call,
}: {
  open: boolean;
  onClose: () => void;
  onDone: () => Promise<void> | void;
  title: string;
  cta: string;
  call: (amount: number) => Promise<unknown>;
}) {
  const toast = useToast();
  const [value, setValue] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) {
      setValue("");
      setBusy(false);
    }
  }, [open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    const amount = Number(value);
    if (!Number.isFinite(amount) || amount <= 0) {
      toast.show("error", "Enter a positive amount");
      return;
    }
    setBusy(true);
    try {
      await call(amount);
      toast.show("success", `${cta} succeeded`);
      onClose();
      await onDone();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={open} onClose={onClose} title={title}>
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="amount">Amount (USD)</label>
          <input
            id="amount"
            type="number"
            inputMode="decimal"
            step="0.01"
            min="0.01"
            autoFocus
            className="input num"
            value={value}
            onChange={(e) => setValue(e.target.value)}
            required
          />
        </div>
        <div className="flex justify-end gap-2 pt-1">
          <button type="button" onClick={onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Working…" : cta}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function DepositDialog(props: { open: boolean; onClose: () => void; onDone: () => Promise<void> | void }) {
  return (
    <AmountDialog
      {...props}
      title="Deposit funds"
      cta="Deposit"
      call={(a) => api.deposit(a)}
    />
  );
}

function WithdrawDialog(props: { open: boolean; onClose: () => void; onDone: () => Promise<void> | void }) {
  return (
    <AmountDialog
      {...props}
      title="Withdraw funds"
      cta="Withdraw"
      call={(a) => api.withdraw(a)}
    />
  );
}

function LoanDialog(props: { open: boolean; onClose: () => void }) {
  const toast = useToast();
  const [value, setValue] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!props.open) { setValue(""); setBusy(false); }
  }, [props.open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    const amount = Number(value);
    if (!Number.isFinite(amount) || amount <= 0) {
      toast.show("error", "Enter a positive amount");
      return;
    }
    setBusy(true);
    try {
      const r = await api.applyLoan(amount);
      toast.show("success", `Loan #${r.loanId} submitted — status PENDING`);
      props.onClose();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={props.open} onClose={props.onClose} title="Apply for a loan">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="loan-amount">Requested amount (USD)</label>
          <input
            id="loan-amount"
            type="number"
            inputMode="decimal"
            step="0.01"
            min="1"
            autoFocus
            className="input num"
            value={value}
            onChange={(e) => setValue(e.target.value)}
            required
          />
          <p className="mt-2 text-xs text-slate2">
            Subject to manager review. You'll be notified once an employee
            processes your application.
          </p>
        </div>
        <div className="flex justify-end gap-2 pt-1">
          <button type="button" onClick={props.onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Submitting…" : "Submit application"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function TransferDialog(props: { open: boolean; onClose: () => void; onDone: () => Promise<void> | void }) {
  const toast = useToast();
  const [target, setTarget] = useState("");
  const [amount, setAmount] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!props.open) { setTarget(""); setAmount(""); setBusy(false); }
  }, [props.open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    const a = Number(amount);
    if (!target.trim()) {
      toast.show("error", "Enter the target username");
      return;
    }
    if (!Number.isFinite(a) || a <= 0) {
      toast.show("error", "Enter a positive amount");
      return;
    }
    setBusy(true);
    try {
      await api.transfer(target.trim(), a);
      toast.show("success", `Transferred ${a.toFixed(2)} to ${target}`);
      props.onClose();
      await props.onDone();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={props.open} onClose={props.onClose} title="Transfer funds">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="target">Recipient username</label>
          <input
            id="target"
            autoFocus
            className="input"
            value={target}
            onChange={(e) => setTarget(e.target.value)}
            required
          />
        </div>
        <div>
          <label className="label" htmlFor="t-amount">Amount (USD)</label>
          <input
            id="t-amount"
            type="number"
            inputMode="decimal"
            step="0.01"
            min="0.01"
            className="input num"
            value={amount}
            onChange={(e) => setAmount(e.target.value)}
            required
          />
        </div>
        <div className="flex justify-end gap-2 pt-1">
          <button type="button" onClick={props.onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Sending…" : "Send transfer"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function FeedbackDialog(props: { open: boolean; onClose: () => void }) {
  const toast = useToast();
  const [msg, setMsg] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!props.open) { setMsg(""); setBusy(false); }
  }, [props.open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    if (!msg.trim()) return;
    setBusy(true);
    try {
      await api.feedback(msg.trim());
      toast.show("success", "Feedback received — thank you.");
      props.onClose();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={props.open} onClose={props.onClose} title="Leave feedback">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="fb">Your note</label>
          <textarea
            id="fb"
            rows={5}
            maxLength={512}
            autoFocus
            className="input"
            value={msg}
            onChange={(e) => setMsg(e.target.value)}
            required
          />
          <div className="mt-1 text-right text-xs text-slate2">
            {msg.length}/512
          </div>
        </div>
        <div className="flex justify-end gap-2">
          <button type="button" onClick={props.onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Sending…" : "Send feedback"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function PasswordDialog(props: { open: boolean; onClose: () => void }) {
  const toast = useToast();
  const [pw, setPw] = useState("");
  const [confirm, setConfirm] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!props.open) { setPw(""); setConfirm(""); setBusy(false); }
  }, [props.open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    if (pw.length < 4) {
      toast.show("error", "Password must be at least 4 characters");
      return;
    }
    if (pw !== confirm) {
      toast.show("error", "Passwords do not match");
      return;
    }
    setBusy(true);
    try {
      await api.changePassword(pw);
      toast.show("success", "Password updated");
      props.onClose();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={props.open} onClose={props.onClose} title="Change password">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="np">New password</label>
          <input
            id="np"
            type="password"
            autoFocus
            className="input"
            value={pw}
            onChange={(e) => setPw(e.target.value)}
            required
          />
        </div>
        <div>
          <label className="label" htmlFor="cp">Confirm</label>
          <input
            id="cp"
            type="password"
            className="input"
            value={confirm}
            onChange={(e) => setConfirm(e.target.value)}
            required
          />
        </div>
        <div className="flex justify-end gap-2">
          <button type="button" onClick={props.onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Updating…" : "Update password"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}
