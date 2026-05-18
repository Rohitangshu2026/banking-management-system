package main

import (
	"encoding/json"
	"net/http"
	"time"
)

const cookieName = "bms_session"

func registerRoutes(mux *http.ServeMux, g *gateway) {
	mux.HandleFunc("GET /api/health", func(w http.ResponseWriter, _ *http.Request) {
		writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
	})

	mux.HandleFunc("POST /api/auth/login", g.handleLogin)
	mux.HandleFunc("POST /api/auth/logout", g.handleLogout)
	mux.HandleFunc("GET /api/me", g.handleMe)

	// Remaining endpoints are listed here as 501s so the API surface is
	// documented in one place and the React side can develop against a
	// stable URL set. Fill them in as the matching bridge transitions
	// land — see gateway/README.md for the punch list.
	stubs := []string{
		"GET /api/transactions",
		"POST /api/deposit",
		"POST /api/withdraw",
		"POST /api/transfer",
		"POST /api/loans",
		"POST /api/feedback",
		"POST /api/password",
		"POST /api/customers",
		"GET /api/customers/{id}",
		"POST /api/loans/{id}/process",
		"POST /api/customers/{id}/toggle",
		"POST /api/loans/{id}/assign",
		"GET /api/feedback",
		"GET /api/users",
		"PATCH /api/users/{id}/role",
	}
	for _, pat := range stubs {
		mux.HandleFunc(pat, notImplemented)
	}
}

func notImplemented(w http.ResponseWriter, r *http.Request) {
	errorJSON(w, http.StatusNotImplemented, "endpoint not wired through to bank_server yet")
}

type loginReq struct {
	Role     string `json:"role"`
	Username string `json:"username"`
	Password string `json:"password"`
}

func (g *gateway) handleLogin(w http.ResponseWriter, r *http.Request) {
	var req loginReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		errorJSON(w, http.StatusBadRequest, "invalid json")
		return
	}
	if req.Username == "" || req.Password == "" || req.Role == "" {
		errorJSON(w, http.StatusBadRequest, "role, username, password are required")
		return
	}

	conn, err := dialBank(g.bankAddr)
	if err != nil {
		errorJSON(w, http.StatusBadGateway, "bank server unreachable")
		return
	}

	if err := conn.authenticate(req.Role, req.Username, req.Password); err != nil {
		_ = conn.Close()
		errorJSON(w, http.StatusUnauthorized, "invalid credentials")
		return
	}

	sess := g.sessions.issue(req.Role, req.Username, conn)
	http.SetCookie(w, &http.Cookie{
		Name:     cookieName,
		Value:    g.sessions.sign(sess.id),
		Path:     "/",
		HttpOnly: true,
		Secure:   r.TLS != nil,
		SameSite: http.SameSiteStrictMode,
		Expires:  time.Now().Add(30 * time.Minute),
	})
	writeJSON(w, http.StatusOK, map[string]any{
		"role":     sess.role,
		"username": sess.username,
	})
}

func (g *gateway) handleLogout(w http.ResponseWriter, r *http.Request) {
	sess := g.requireSession(w, r)
	if sess == nil {
		return
	}
	g.sessions.drop(sess.id)
	http.SetCookie(w, &http.Cookie{
		Name:     cookieName,
		Value:    "",
		Path:     "/",
		HttpOnly: true,
		MaxAge:   -1,
	})
	writeJSON(w, http.StatusOK, map[string]string{"status": "logged out"})
}

func (g *gateway) handleMe(w http.ResponseWriter, r *http.Request) {
	sess := g.requireSession(w, r)
	if sess == nil {
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{
		"role":     sess.role,
		"username": sess.username,
	})
}

func (g *gateway) requireSession(w http.ResponseWriter, r *http.Request) *session {
	c, err := r.Cookie(cookieName)
	if err != nil {
		errorJSON(w, http.StatusUnauthorized, "not logged in")
		return nil
	}
	id := g.sessions.verify(c.Value)
	if id == "" {
		errorJSON(w, http.StatusUnauthorized, "invalid session")
		return nil
	}
	sess := g.sessions.get(id)
	if sess == nil {
		errorJSON(w, http.StatusUnauthorized, "session expired")
		return nil
	}
	return sess
}
