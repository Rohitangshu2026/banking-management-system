import { useCallback, useEffect, useState } from "react";
import AppShell from "../components/AppShell";
import Dialog from "../components/Dialog";
import { useToast } from "../components/Toast";
import { api, ApiError, type AssignedLoan } from "../lib/api";
import { fmtAccount, fmtMoney } from "../lib/format";

const nav = [
  { label: "Loan queue", to: "/employee" },
  { label: "Settings", to: "/employee/settings" },
];

export default function EmployeeConsole() {
  const toast = useToast();
  const [loans, setLoans] = useState<AssignedLoan[] | null>(null);
  const [addOpen, setAddOpen] = useState(false);
  const [busyId, setBusyId] = useState<number | null>(null);

  const refresh = useCallback(async () => {
    try {
      setLoans(await api.assignedLoans());
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    }
  }, [toast]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  async function decide(id: number, action: "approve" | "reject") {
    setBusyId(id);
    try {
      await api.processLoan(id, action);
      toast.show("success", `Loan #${id} ${action === "approve" ? "approved" : "rejected"}`);
      await refresh();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusyId(null);
    }
  }

  return (
    <AppShell navItems={nav} title="Loan queue">
      <div className="flex justify-end mb-4">
        <button className="btn-primary" onClick={() => setAddOpen(true)}>
          Open new customer account
        </button>
      </div>

      <div className="plate overflow-hidden">
        {loans === null ? (
          <div className="px-6 py-10 text-center text-sm text-slate2">Loading…</div>
        ) : loans.length === 0 ? (
          <div className="px-6 py-10 text-center text-sm text-slate2">
            No pending loans assigned to you. When a manager assigns one,
            it'll appear here.
          </div>
        ) : (
          <table className="w-full text-sm">
            <thead className="bg-ink/[0.025] text-left text-xs uppercase tracking-[0.1em] text-slate2">
              <tr>
                <th className="px-5 py-3 font-medium">Loan ID</th>
                <th className="px-5 py-3 font-medium">Customer</th>
                <th className="px-5 py-3 font-medium">Account</th>
                <th className="px-5 py-3 font-medium text-right">Amount</th>
                <th className="px-5 py-3 font-medium text-right">Actions</th>
              </tr>
            </thead>
            <tbody>
              {loans.map((l, i) => (
                <tr key={l.loanId} className={i % 2 ? "bg-parchment/40" : ""}>
                  <td className="px-5 py-3 num">{l.loanId}</td>
                  <td className="px-5 py-3">{l.customerUsername}</td>
                  <td className="px-5 py-3 num">{fmtAccount(l.accountId)}</td>
                  <td className="px-5 py-3 text-right num">{fmtMoney(l.amount)}</td>
                  <td className="px-5 py-3 text-right">
                    <div className="inline-flex gap-2">
                      <button
                        disabled={busyId === l.loanId}
                        onClick={() => void decide(l.loanId, "approve")}
                        className="btn-primary py-1.5 px-3 text-xs"
                      >
                        Approve
                      </button>
                      <button
                        disabled={busyId === l.loanId}
                        onClick={() => void decide(l.loanId, "reject")}
                        className="btn-ghost py-1.5 px-3 text-xs"
                      >
                        Reject
                      </button>
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      <AddCustomerDialog
        open={addOpen}
        onClose={() => setAddOpen(false)}
      />
    </AppShell>
  );
}

function AddCustomerDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const toast = useToast();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [deposit, setDeposit] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) { setUsername(""); setPassword(""); setDeposit(""); setBusy(false); }
  }, [open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    const amt = Number(deposit);
    if (!Number.isFinite(amt) || amt <= 0) {
      toast.show("error", "Initial deposit must be positive");
      return;
    }
    setBusy(true);
    try {
      const r = await api.addCustomer(username.trim(), password, amt);
      toast.show("success",
        `Created ${r.username} (user #${r.userId}, account ${r.accountId})`);
      onClose();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={open} onClose={onClose} title="Open new customer account">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="nu">Username</label>
          <input id="nu" autoFocus className="input"
                 value={username} onChange={(e) => setUsername(e.target.value)} required />
        </div>
        <div>
          <label className="label" htmlFor="np">Initial password</label>
          <input id="np" type="password" className="input"
                 value={password} onChange={(e) => setPassword(e.target.value)} required />
        </div>
        <div>
          <label className="label" htmlFor="nd">Initial deposit (USD)</label>
          <input id="nd" type="number" inputMode="decimal" step="0.01" min="0.01"
                 className="input num"
                 value={deposit} onChange={(e) => setDeposit(e.target.value)} required />
        </div>
        <div className="flex justify-end gap-2">
          <button type="button" onClick={onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Creating…" : "Create account"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}
