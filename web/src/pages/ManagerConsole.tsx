import { useCallback, useEffect, useState } from "react";
import AppShell from "../components/AppShell";
import Dialog from "../components/Dialog";
import { useToast } from "../components/Toast";
import { api, ApiError, type Feedback, type PendingLoan } from "../lib/api";
import { fmtAccount, fmtDate, fmtMoney, fmtTime } from "../lib/format";

const nav = [
  { label: "Loan assignment", to: "/manager" },
  { label: "Customer status", to: "/manager/customers" },
  { label: "Feedback inbox", to: "/manager/feedback" },
];

type Tab = "loans" | "customers" | "feedback";

export default function ManagerConsole() {
  const [tab, setTab] = useState<Tab>("loans");
  return (
    <AppShell navItems={nav} title="Manager console">
      <div className="flex gap-2 mb-6">
        <TabBtn label="Loan assignment" active={tab === "loans"} on={() => setTab("loans")} />
        <TabBtn label="Customer status" active={tab === "customers"} on={() => setTab("customers")} />
        <TabBtn label="Feedback inbox" active={tab === "feedback"} on={() => setTab("feedback")} />
      </div>

      {tab === "loans" && <LoansTab />}
      {tab === "customers" && <CustomersTab />}
      {tab === "feedback" && <FeedbackTab />}
    </AppShell>
  );
}

function TabBtn({ label, active, on }: { label: string; active: boolean; on: () => void }) {
  return (
    <button
      onClick={on}
      className={`rounded-md px-4 py-2 text-sm transition-colors ${
        active ? "bg-ink text-parchment" : "border border-ink/15 text-ink hover:bg-ink/5"
      }`}
    >
      {label}
    </button>
  );
}

function LoansTab() {
  const toast = useToast();
  const [loans, setLoans] = useState<PendingLoan[] | null>(null);
  const [assignFor, setAssignFor] = useState<PendingLoan | null>(null);

  const refresh = useCallback(async () => {
    try {
      setLoans(await api.pendingLoans());
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    }
  }, [toast]);

  useEffect(() => { void refresh(); }, [refresh]);

  return (
    <>
      <div className="plate overflow-hidden">
        {loans === null ? (
          <div className="px-6 py-10 text-center text-sm text-slate2">Loading…</div>
        ) : loans.length === 0 ? (
          <div className="px-6 py-10 text-center text-sm text-slate2">
            No unassigned pending loans.
          </div>
        ) : (
          <table className="w-full text-sm">
            <thead className="bg-ink/[0.025] text-left text-xs uppercase tracking-[0.1em] text-slate2">
              <tr>
                <th className="px-5 py-3 font-medium">Loan ID</th>
                <th className="px-5 py-3 font-medium">Account</th>
                <th className="px-5 py-3 font-medium text-right">Amount</th>
                <th className="px-5 py-3 font-medium text-right">Actions</th>
              </tr>
            </thead>
            <tbody>
              {loans.map((l, i) => (
                <tr key={l.loanId} className={i % 2 ? "bg-parchment/40" : ""}>
                  <td className="px-5 py-3 num">{l.loanId}</td>
                  <td className="px-5 py-3 num">{fmtAccount(l.accountId)}</td>
                  <td className="px-5 py-3 text-right num">{fmtMoney(l.amount)}</td>
                  <td className="px-5 py-3 text-right">
                    <button
                      className="btn-primary py-1.5 px-3 text-xs"
                      onClick={() => setAssignFor(l)}
                    >
                      Assign
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
      <AssignDialog
        loan={assignFor}
        onClose={() => setAssignFor(null)}
        onDone={refresh}
      />
    </>
  );
}

function AssignDialog({
  loan,
  onClose,
  onDone,
}: {
  loan: PendingLoan | null;
  onClose: () => void;
  onDone: () => Promise<void> | void;
}) {
  const toast = useToast();
  const [emp, setEmp] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!loan) { setEmp(""); setBusy(false); }
  }, [loan]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    if (!loan) return;
    setBusy(true);
    try {
      await api.assignLoan(loan.loanId, emp.trim());
      toast.show("success", `Loan #${loan.loanId} assigned to ${emp}`);
      onClose();
      await onDone();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog
      open={loan !== null}
      onClose={onClose}
      title={loan ? `Assign loan #${loan.loanId}` : ""}
    >
      <form onSubmit={submit} className="space-y-4">
        {loan && (
          <p className="text-sm text-slate2">
            Account {fmtAccount(loan.accountId)} · {fmtMoney(loan.amount)}
          </p>
        )}
        <div>
          <label className="label" htmlFor="emp">Employee username</label>
          <input
            id="emp"
            autoFocus
            className="input"
            value={emp}
            onChange={(e) => setEmp(e.target.value)}
            required
          />
        </div>
        <div className="flex justify-end gap-2">
          <button type="button" onClick={onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Assigning…" : "Assign"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function CustomersTab() {
  const toast = useToast();
  const [username, setUsername] = useState("");
  const [busy, setBusy] = useState(false);
  const [last, setLast] = useState<{ name: string; active: boolean } | null>(null);

  async function toggle(e: React.FormEvent) {
    e.preventDefault();
    if (!username.trim()) return;
    setBusy(true);
    try {
      const r = await api.toggleCustomer(username.trim());
      setLast({ name: r.username, active: r.isActive });
      toast.show(
        "success",
        `${r.username} is now ${r.isActive ? "active" : "inactive"}`
      );
      setUsername("");
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="plate p-7 max-w-xl">
      <h3 className="font-display text-xl text-ink">Toggle customer status</h3>
      <p className="mt-2 text-sm text-slate2">
        Toggling a customer flips their active flag. Inactive customers are
        kicked from the bank server on their next operation.
      </p>
      <form onSubmit={toggle} className="mt-5 flex gap-2">
        <input
          className="input flex-1"
          placeholder="Customer username"
          value={username}
          onChange={(e) => setUsername(e.target.value)}
        />
        <button type="submit" disabled={busy} className="btn-primary">
          {busy ? "Working…" : "Toggle"}
        </button>
      </form>
      {last && (
        <div className="mt-4 text-sm text-slate2">
          Last action: <span className="text-ink">{last.name}</span> →{" "}
          {last.active ? "active" : "inactive"}
        </div>
      )}
    </div>
  );
}

function FeedbackTab() {
  const toast = useToast();
  const [items, setItems] = useState<Feedback[] | null>(null);

  useEffect(() => {
    api.feedbackInbox()
      .then(setItems)
      .catch((err) =>
        toast.show("error", err instanceof ApiError ? err.message : "Failed")
      );
  }, [toast]);

  if (items === null) {
    return <div className="plate p-10 text-center text-sm text-slate2">Loading…</div>;
  }
  if (items.length === 0) {
    return (
      <div className="plate p-10 text-center text-sm text-slate2">
        No feedback yet.
      </div>
    );
  }

  return (
    <div className="space-y-4">
      {items.map((f, i) => (
        <div key={`${f.ts}-${i}`} className="plate p-6">
          <div className="flex justify-between text-sm text-slate2">
            <span>
              From <span className="text-ink font-medium">{f.username}</span>{" "}
              · user #{f.userId}
            </span>
            <span>
              {fmtDate(f.ts)} · {fmtTime(f.ts)}
            </span>
          </div>
          <div className="rule-gilded mt-3" />
          <p className="mt-3 text-ink whitespace-pre-wrap">{f.message}</p>
        </div>
      ))}
    </div>
  );
}
