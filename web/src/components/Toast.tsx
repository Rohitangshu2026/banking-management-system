import { createContext, useCallback, useContext, useEffect, useState } from "react";

type Tone = "info" | "success" | "error";
interface ToastItem {
  id: number;
  tone: Tone;
  message: string;
}

interface ToastCtx {
  show: (tone: Tone, message: string) => void;
}

const Ctx = createContext<ToastCtx | null>(null);

let nextId = 1;

export function ToastProvider({ children }: { children: React.ReactNode }) {
  const [items, setItems] = useState<ToastItem[]>([]);

  const show = useCallback((tone: Tone, message: string) => {
    const id = nextId++;
    setItems((prev) => [...prev, { id, tone, message }]);
    setTimeout(() => {
      setItems((prev) => prev.filter((t) => t.id !== id));
    }, 4000);
  }, []);

  return (
    <Ctx.Provider value={{ show }}>
      {children}
      <div className="fixed bottom-6 right-6 z-[60] flex flex-col gap-2 items-end">
        {items.map((t) => (
          <ToastView key={t.id} item={t} />
        ))}
      </div>
    </Ctx.Provider>
  );
}

function ToastView({ item }: { item: ToastItem }) {
  const [shown, setShown] = useState(false);
  useEffect(() => {
    const r = requestAnimationFrame(() => setShown(true));
    return () => cancelAnimationFrame(r);
  }, []);
  const palette = {
    info: "border-ink/15 bg-parchment-soft text-ink",
    success: "border-emerald-700/30 bg-emerald-50 text-emerald-900",
    error: "border-red-700/30 bg-red-50 text-red-900",
  }[item.tone];

  return (
    <div
      className={`rounded-md border px-4 py-3 text-sm shadow-plate transition-all duration-200 ${palette} ${
        shown ? "opacity-100 translate-y-0" : "opacity-0 translate-y-2"
      }`}
    >
      {item.message}
    </div>
  );
}

export function useToast() {
  const v = useContext(Ctx);
  if (!v) throw new Error("useToast must be inside <ToastProvider>");
  return v;
}
