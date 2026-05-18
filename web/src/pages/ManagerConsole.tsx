import AppShell from "../components/AppShell";

const nav = [
  { label: "Loan assignment", to: "/manager" },
  { label: "Customer status", to: "/manager/customers" },
  { label: "Feedback inbox", to: "/manager/feedback" },
  { label: "Settings", to: "/manager/settings" },
];

export default function ManagerConsole() {
  return (
    <AppShell navItems={nav} title="Loan assignment">
      <div className="plate p-10 text-center">
        <div className="font-display text-2xl text-ink">
          Unassigned loan applications
        </div>
        <p className="mt-3 text-slate2 max-w-md mx-auto">
          Each pending loan needs an employee assigned. Once the gateway
          exposes /api/loans/{"{id}"}/assign, this view will list applicants
          and the available employees side by side.
        </p>
        <div className="rule-gilded mt-7" />
      </div>
    </AppShell>
  );
}
