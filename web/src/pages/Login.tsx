import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { api, ApiError, type Role } from "../lib/api";
import { useAuth } from "../auth";

const ROLES: { key: Role; label: string }[] = [
  { key: "customer", label: "Customer" },
  { key: "employee", label: "Employee" },
  { key: "manager", label: "Manager" },
  { key: "admin", label: "Administrator" },
];

export default function Login() {
  const nav = useNavigate();
  const { refresh } = useAuth();
  const [role, setRole] = useState<Role>("customer");
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  async function onSubmit(e: React.FormEvent) {
    e.preventDefault();
    setError(null);
    setBusy(true);
    try {
      await api.login(role, username, password);
      await refresh();
      nav(`/${role}`, { replace: true });
    } catch (err) {
      const msg =
        err instanceof ApiError ? err.message : "Something went wrong";
      setError(msg);
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="min-h-screen flex items-center justify-center px-4">
      <div className="w-full max-w-md">
        <div className="text-center mb-8">
          <div className="inline-flex h-14 w-14 items-center justify-center rounded-full border border-gilded/60">
            <span className="font-display text-2xl text-gilded-deep">H</span>
          </div>
          <h1 className="mt-5 font-display text-4xl text-ink">Heritage Bank</h1>
          <p className="mt-2 text-sm text-slate2">
            Private banking, since eighteen ninety-three.
          </p>
          <div className="rule-gilded mt-6" />
        </div>

        <form onSubmit={onSubmit} className="plate p-8 space-y-5">
          <div>
            <label className="label">Role</label>
            <div className="grid grid-cols-2 gap-2">
              {ROLES.map((r) => (
                <button
                  type="button"
                  key={r.key}
                  onClick={() => setRole(r.key)}
                  className={`rounded-md px-3 py-2 text-sm transition-colors ${
                    role === r.key
                      ? "bg-ink text-parchment"
                      : "border border-ink/15 text-ink hover:bg-ink/5"
                  }`}
                >
                  {r.label}
                </button>
              ))}
            </div>
          </div>

          <div>
            <label className="label" htmlFor="u">
              Username
            </label>
            <input
              id="u"
              className="input"
              autoComplete="username"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              required
            />
          </div>

          <div>
            <label className="label" htmlFor="p">
              Password
            </label>
            <input
              id="p"
              type="password"
              className="input"
              autoComplete="current-password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              required
            />
          </div>

          {error && (
            <div className="rounded-md border border-red-500/30 bg-red-50 px-3 py-2 text-sm text-red-900">
              {error}
            </div>
          )}

          <button type="submit" disabled={busy} className="btn-primary w-full">
            {busy ? "Signing in…" : "Sign in"}
          </button>

          <p className="text-center text-xs text-slate2">
            Protected by mutually authenticated TLS. By signing in you accept
            our acceptable-use policy.
          </p>
        </form>
      </div>
    </div>
  );
}
