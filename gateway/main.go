// Package main is the HTTP/JSON gateway that fronts the bank_server.
//
// The bank_server speaks a line-oriented, menu-driven TCP protocol that is
// awkward to consume from a browser. This gateway maintains one TCP
// connection to the bank_server per logged-in browser session, advances
// the menu state machine on the user's behalf, and exposes the result as
// a small JSON+WebSocket API.
//
// Design notes:
//
//   - Each browser session is bound to a long-lived bridge.Conn. The
//     gateway keeps the underlying TCP socket open between requests so we
//     don't have to re-authenticate on every API call.
//   - The session cookie is signed with HMAC-SHA256 over an opaque
//     session id. The id maps to an in-memory entry in sessionStore.
//     Restarting the gateway invalidates all sessions, which is fine for
//     now; if persistence is needed later, switch to a Redis-backed store.
//   - Per-IP rate limiting is intentionally applied here, not in the C
//     server, because rewriting the listen loop to track per-IP buckets
//     in C is more code than it's worth when a sidecar can do it.
//
// The gateway is incomplete. The login + /api/me round-trip is wired end
// to end as the reference shape; the rest of the endpoints listed in
// routes.go return 501 until the menu transitions in bridge.go are
// filled in. See gateway/README.md for the punch list.
package main

import (
	"flag"
	"log"
	"net/http"
	"os"
	"time"
)

func main() {
	var (
		listen   = flag.String("listen", envOr("GATEWAY_LISTEN", ":8443"),
			"HTTP listen address")
		bankAddr = flag.String("bank", envOr("BANK_ADDR", "127.0.0.1:8080"),
			"Bank server address (TCP)")
		webDir = flag.String("web", envOr("GATEWAY_WEB_DIR", "../web/dist"),
			"Static assets root for the React app; disabled if missing")
		certFile = flag.String("cert", envOr("GATEWAY_TLS_CERT", ""),
			"TLS certificate (PEM); plain HTTP if empty")
		keyFile = flag.String("key", envOr("GATEWAY_TLS_KEY", ""), "TLS key (PEM)")
	)
	flag.Parse()

	store := newSessionStore()
	g := &gateway{
		bankAddr: *bankAddr,
		sessions: store,
	}

	mux := http.NewServeMux()
	registerRoutes(mux, g)

	// Serve the React build alongside the API if it exists.
	if _, err := os.Stat(*webDir); err == nil {
		log.Printf("serving static assets from %s", *webDir)
		mux.Handle("/", http.FileServer(http.Dir(*webDir)))
	}

	srv := &http.Server{
		Addr:              *listen,
		Handler:           withLogging(withRateLimit(mux)),
		ReadHeaderTimeout: 5 * time.Second,
		WriteTimeout:      30 * time.Second,
		IdleTimeout:       120 * time.Second,
	}

	if *certFile != "" && *keyFile != "" {
		log.Printf("listening on https://%s", *listen)
		log.Fatal(srv.ListenAndServeTLS(*certFile, *keyFile))
	}
	log.Printf("listening on http://%s (NO TLS — set -cert/-key for production)", *listen)
	log.Fatal(srv.ListenAndServe())
}

func envOr(key, dflt string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return dflt
}
