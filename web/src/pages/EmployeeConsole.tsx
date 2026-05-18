import AppShell from "../components/AppShell";

const nav = [
  { label: "Loan queue", to: "/employee" },
  { label: "Customers", to: "/employee/customers" },
  { label: "Transactions", to: "/employee/transactions" },
  { label: "Settings", to: "/employee/settings" },
];

export default function EmployeeConsole() {
  return (
    <AppShell navItems={nav} title="Loan queue">
      <Placeholder
        heading="Pending loan applications"
        body="Once /api/loans?status=PENDING is wired in the gateway, applications will appear here with approve / reject actions."
      />
    </AppShell>
  );
}

function Placeholder({ heading, body }: { heading: string; body: string }) {
  return (
    <div className="plate p-10 text-center">
      <div className="font-display text-2xl text-ink">{heading}</div>
      <p className="mt-3 text-slate2 max-w-md mx-auto">{body}</p>
      <div className="rule-gilded mt-7" />
      <div className="mt-7 text-xs text-slate2">
        See gateway/README.md for the punch list.
      </div>
    </div>
  );
}
