// Single source of truth for money / account-id formatting.
// Anything that displays a number in the app should go through here.

const usd = new Intl.NumberFormat("en-US", {
  style: "currency",
  currency: "USD",
  minimumFractionDigits: 2,
});

export const fmtMoney = (n: number) => usd.format(n);

export const fmtAccount = (id: number | null | undefined) => {
  if (id == null || id < 0 || !Number.isFinite(id)) return "—";
  // Pad to 10 chars so account numbers align in tables.
  return String(id).padStart(10, "0");
};

export const fmtDate = (d: Date | string | number) => {
  const date = typeof d === "object" ? d : new Date(d);
  return date.toLocaleDateString("en-US", {
    year: "numeric",
    month: "short",
    day: "2-digit",
  });
};

export const fmtTime = (d: Date | string | number) => {
  const date = typeof d === "object" ? d : new Date(d);
  return date.toLocaleTimeString("en-US", {
    hour: "2-digit",
    minute: "2-digit",
  });
};
