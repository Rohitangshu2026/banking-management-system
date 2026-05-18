import { Link, useLocation } from "react-router-dom";
import { useAuth } from "../auth";

interface NavItem {
  label: string;
  to: string;
}

interface Props {
  navItems: NavItem[];
  title: string;
  children: React.ReactNode;
}

export default function AppShell({ navItems, title, children }: Props) {
  const { me, logout } = useAuth();
  const loc = useLocation();

  return (
    <div className="min-h-screen flex">
      <aside className="hidden md:flex w-64 flex-col border-r border-ink/10 bg-parchment-soft/60 backdrop-blur">
        <div className="px-6 py-7">
          <div className="font-display text-xl text-ink">Heritage Bank</div>
          <div className="mt-1 text-[11px] uppercase tracking-[0.18em] text-slate2">
            Est. 1893
          </div>
          <div className="rule-gilded mt-5" />
        </div>

        <nav className="flex-1 px-3 space-y-1">
          {navItems.map((n) => {
            const active =
              loc.pathname === n.to ||
              (n.to !== "/" && loc.pathname.startsWith(n.to));
            return (
              <Link
                key={n.to}
                to={n.to}
                className={`block rounded-md px-3 py-2 text-sm transition-colors ${
                  active
                    ? "bg-ink text-parchment"
                    : "text-ink/80 hover:bg-ink/5"
                }`}
              >
                {n.label}
              </Link>
            );
          })}
        </nav>

        <div className="px-6 py-5 border-t border-ink/10">
          <div className="text-xs text-slate2">Signed in as</div>
          <div className="mt-0.5 font-medium">{me?.username ?? "—"}</div>
          <div className="text-[11px] text-slate2 capitalize">{me?.role}</div>
          <button
            onClick={() => void logout()}
            className="mt-4 btn-ghost w-full"
          >
            Sign out
          </button>
        </div>
      </aside>

      <main className="flex-1 min-w-0">
        <header className="px-8 py-6 border-b border-ink/10">
          <h1 className="font-display text-3xl text-ink">{title}</h1>
        </header>
        <div className="px-8 py-8 max-w-6xl">{children}</div>
      </main>
    </div>
  );
}
