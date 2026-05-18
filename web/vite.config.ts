import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// During dev, proxy /api to the Go gateway running on :8443 (or :8080
// if TLS is disabled). The production build is served by the gateway
// itself, so no proxy is needed once vite build has emitted dist/.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      "/api": {
        target: process.env.VITE_API_TARGET ?? "http://127.0.0.1:8443",
        changeOrigin: true,
        secure: false,
      },
    },
  },
});
