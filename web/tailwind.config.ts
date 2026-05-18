import type { Config } from "tailwindcss";

// Classic-elegant palette: deep ink, parchment, gilded accent.
// Serif for display, sans for body, mono for numerics.
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        ink: {
          DEFAULT: "#0B1220",
          soft: "#1A2236",
          muted: "#3D4660",
        },
        parchment: {
          DEFAULT: "#F7F3EC",
          soft: "#FBF8F2",
          deep: "#EAE2D3",
        },
        gilded: {
          DEFAULT: "#B08D57",
          deep: "#8A6B3D",
          soft: "#D6BB8A",
        },
        slate2: "#5C6478",
      },
      fontFamily: {
        display: ["'Fraunces'", "ui-serif", "Georgia", "serif"],
        sans: ["'Inter'", "ui-sans-serif", "system-ui", "sans-serif"],
        mono: ["'JetBrains Mono'", "ui-monospace", "monospace"],
      },
      boxShadow: {
        hairline: "0 0 0 1px rgba(11, 18, 32, 0.08)",
        plate: "0 1px 0 rgba(11,18,32,0.04), 0 0 0 1px rgba(11,18,32,0.06)",
      },
      letterSpacing: {
        tightish: "-0.012em",
      },
    },
  },
  plugins: [],
} satisfies Config;
