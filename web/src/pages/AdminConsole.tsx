import { useCallback, useEffect, useState } from "react";
import AppShell from "../components/AppShell";
import Dialog from "../components/Dialog";
import { useToast } from "../components/Toast";
import { api, ApiError } from "../lib/api";

const nav = [
  { label: "Console", to: "/admin" },
];

type Tab = "users" | "audit";

export default function AdminConsole() {
  const [tab, setTab] = useState<Tab>("users");
  return (
    <AppShell navItems={nav} title="Admin console">
      <div className="flex gap-2 mb-6">
        <TabBtn label="Users" active={tab === "users"} on={() => setTab("users")} />
        <TabBtn label="Audit log" active={tab === "audit"} on={() => setTab("audit")} />
      </div>
      {tab === "users" && <UsersTab />}
      {tab === "audit" && <AuditTab />}
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

function UsersTab() {
  const [addOpen, setAddOpen] = useState(false);
  const [modOpen, setModOpen] = useState(false);
  const [roleOpen, setRoleOpen] = useState(false);

  return (
    <div className="space-y-6">
      <div className="plate p-7">
        <h3 className="font-display text-xl text-ink">User management</h3>
        <p className="mt-2 text-sm text-slate2">
          Add new employees and managers, modify existing user records, and
          swap a user's role. Every action is recorded in the audit log.
        </p>
        <div className="rule-gilded mt-5" />
        <div className="mt-5 flex flex-wrap gap-3">
          <button className="btn-primary" onClick={() => setAddOpen(true)}>
            Add employee or manager
          </button>
          <button className="btn-ghost" onClick={() => setModOpen(true)}>
            Modify user
          </button>
          <button className="btn-ghost" onClick={() => setRoleOpen(true)}>
            Change role
          </button>
        </div>
      </div>

      <AddUserDialog open={addOpen} onClose={() => setAddOpen(false)} />
      <ModifyUserDialog open={modOpen} onClose={() => setModOpen(false)} />
      <ChangeRoleDialog open={roleOpen} onClose={() => setRoleOpen(false)} />
    </div>
  );
}

function AddUserDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const toast = useToast();
  const [u, setU] = useState("");
  const [p, setP] = useState("");
  const [r, setR] = useState<"employee" | "manager">("employee");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) { setU(""); setP(""); setR("employee"); setBusy(false); }
  }, [open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    try {
      const res = await api.addUser(u.trim(), p, r);
      toast.show("success",
        `Created ${res.role} ${res.username}${res.employeeId > 0 ? ` (ID ${res.employeeId})` : ""}`);
      onClose();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={open} onClose={onClose} title="Add employee or manager">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label">Role</label>
          <div className="grid grid-cols-2 gap-2">
            {(["employee", "manager"] as const).map((opt) => (
              <button
                type="button"
                key={opt}
                onClick={() => setR(opt)}
                className={`rounded-md px-3 py-2 text-sm transition-colors capitalize ${
                  r === opt
                    ? "bg-ink text-parchment"
                    : "border border-ink/15 text-ink hover:bg-ink/5"
                }`}
              >
                {opt}
              </button>
            ))}
          </div>
        </div>
        <div>
          <label className="label" htmlFor="au">Username</label>
          <input id="au" autoFocus className="input"
                 value={u} onChange={(e) => setU(e.target.value)} required />
        </div>
        <div>
          <label className="label" htmlFor="ap">Password</label>
          <input id="ap" type="password" className="input"
                 value={p} onChange={(e) => setP(e.target.value)} required />
        </div>
        <div className="flex justify-end gap-2">
          <button type="button" onClick={onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Creating…" : "Create"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function ModifyUserDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const toast = useToast();
  const [target, setTarget] = useState("");
  const [nu, setNu] = useState("");
  const [np, setNp] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) { setTarget(""); setNu(""); setNp(""); setBusy(false); }
  }, [open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    try {
      await api.modifyUser(target.trim(), nu || undefined, np || undefined);
      toast.show("success", `Updated ${target}`);
      onClose();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={open} onClose={onClose} title="Modify user">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="mt">Username to modify</label>
          <input id="mt" autoFocus className="input"
                 value={target} onChange={(e) => setTarget(e.target.value)} required />
        </div>
        <div>
          <label className="label" htmlFor="mu">New username (optional)</label>
          <input id="mu" className="input"
                 value={nu} onChange={(e) => setNu(e.target.value)} />
        </div>
        <div>
          <label className="label" htmlFor="mp">New password (optional)</label>
          <input id="mp" type="password" className="input"
                 value={np} onChange={(e) => setNp(e.target.value)} />
        </div>
        <div className="flex justify-end gap-2">
          <button type="button" onClick={onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Saving…" : "Save"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function ChangeRoleDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const toast = useToast();
  const [u, setU] = useState("");
  const [r, setR] = useState<"employee" | "manager">("employee");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) { setU(""); setR("employee"); setBusy(false); }
  }, [open]);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    try {
      const res = await api.changeRole(u.trim(), r);
      toast.show("success", `${res.username} is now ${res.newRole}`);
      onClose();
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <Dialog open={open} onClose={onClose} title="Change user role">
      <form onSubmit={submit} className="space-y-4">
        <div>
          <label className="label" htmlFor="ru">Username</label>
          <input id="ru" autoFocus className="input"
                 value={u} onChange={(e) => setU(e.target.value)} required />
        </div>
        <div>
          <label className="label">New role</label>
          <div className="grid grid-cols-2 gap-2">
            {(["employee", "manager"] as const).map((opt) => (
              <button
                type="button"
                key={opt}
                onClick={() => setR(opt)}
                className={`rounded-md px-3 py-2 text-sm transition-colors capitalize ${
                  r === opt
                    ? "bg-ink text-parchment"
                    : "border border-ink/15 text-ink hover:bg-ink/5"
                }`}
              >
                {opt}
              </button>
            ))}
          </div>
        </div>
        <div className="flex justify-end gap-2">
          <button type="button" onClick={onClose} className="btn-ghost">Cancel</button>
          <button type="submit" disabled={busy} className="btn-primary">
            {busy ? "Updating…" : "Update"}
          </button>
        </div>
      </form>
    </Dialog>
  );
}

function AuditTab() {
  const toast = useToast();
  const [text, setText] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    try {
      const r = await api.logs();
      setText(r.text);
    } catch (err) {
      toast.show("error", err instanceof ApiError ? err.message : "Failed");
    }
  }, [toast]);

  useEffect(() => { void refresh(); }, [refresh]);

  return (
    <div className="plate overflow-hidden">
      <div className="px-5 py-3 flex justify-between items-center border-b border-ink/10">
        <span className="text-xs uppercase tracking-[0.14em] text-slate2">
          data/logs.txt
        </span>
        <button onClick={() => void refresh()} className="text-sm text-gilded-deep hover:underline">
          Refresh
        </button>
      </div>
      <pre className="px-5 py-4 text-xs num text-ink/80 overflow-x-auto whitespace-pre-wrap min-h-[10rem]">
        {text === null ? "Loading…" : text || "(empty)"}
      </pre>
    </div>
  );
}
