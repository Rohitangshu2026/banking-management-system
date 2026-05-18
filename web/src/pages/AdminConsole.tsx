import AppShell from "../components/AppShell";

const nav = [
  { label: "User directory", to: "/admin" },
  { label: "Roles", to: "/admin/roles" },
  { label: "Audit log", to: "/admin/audit" },
  { label: "Settings", to: "/admin/settings" },
];

export default function AdminConsole() {
  return (
    <AppShell navItems={nav} title="User directory">
      <div className="plate p-10 text-center">
        <div className="font-display text-2xl text-ink">All users</div>
        <p className="mt-3 text-slate2 max-w-md mx-auto">
          Wires to /api/users; supports adding employees, demoting managers,
          and resetting credentials. Audit entries flow into /admin/audit
          once the gateway exposes the existing data/logs.txt stream.
        </p>
        <div className="rule-gilded mt-7" />
      </div>
    </AppShell>
  );
}
