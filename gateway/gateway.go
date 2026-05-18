package main

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"net/http"
	"sync"
	"time"
)

// gateway is the request-level dependency bundle. Handlers receive it
// through closures registered in routes.go.
type gateway struct {
	bankAddr string
	sessions *sessionStore
}

// session holds the per-browser state: the TCP connection to the bank
// server (advanced through its menu state machine) plus identity metadata
// captured at login time.
type session struct {
	id         string
	role       string
	username   string
	createdAt  time.Time
	lastUsedAt time.Time

	// conn is the live TCP connection to the bank server. nil until the
	// user has successfully authenticated.
	conn *bankConn
}

type sessionStore struct {
	mu       sync.Mutex
	entries  map[string]*session
	hmacKey  []byte
	idleKill time.Duration
}

func newSessionStore() *sessionStore {
	// 32 random bytes is enough entropy for a cookie-signing key.
	// We deliberately don't persist it: gateway restarts log everyone out.
	key := make([]byte, 32)
	if _, err := rand.Read(key); err != nil {
		panic(err) // crypto/rand failing means the host is unusable
	}
	return &sessionStore{
		entries:  make(map[string]*session),
		hmacKey:  key,
		idleKill: 30 * time.Minute,
	}
}

func (s *sessionStore) issue(role, username string, conn *bankConn) *session {
	id := newSessionID()
	now := time.Now()
	sess := &session{
		id:         id,
		role:       role,
		username:   username,
		createdAt:  now,
		lastUsedAt: now,
		conn:       conn,
	}
	s.mu.Lock()
	s.entries[id] = sess
	s.mu.Unlock()
	return sess
}

func (s *sessionStore) get(id string) *session {
	s.mu.Lock()
	defer s.mu.Unlock()
	sess, ok := s.entries[id]
	if !ok {
		return nil
	}
	if time.Since(sess.lastUsedAt) > s.idleKill {
		delete(s.entries, id)
		if sess.conn != nil {
			_ = sess.conn.Close()
		}
		return nil
	}
	sess.lastUsedAt = time.Now()
	return sess
}

func (s *sessionStore) drop(id string) {
	s.mu.Lock()
	sess, ok := s.entries[id]
	if ok {
		delete(s.entries, id)
	}
	s.mu.Unlock()
	if ok && sess.conn != nil {
		_ = sess.conn.Close()
	}
}

func (s *sessionStore) sign(id string) string {
	mac := hmac.New(sha256.New, s.hmacKey)
	mac.Write([]byte(id))
	return id + "." + hex.EncodeToString(mac.Sum(nil))
}

func (s *sessionStore) verify(cookieVal string) string {
	// Expected format: <id>.<hex-hmac>
	for i := 0; i < len(cookieVal); i++ {
		if cookieVal[i] == '.' {
			id, sig := cookieVal[:i], cookieVal[i+1:]
			mac := hmac.New(sha256.New, s.hmacKey)
			mac.Write([]byte(id))
			want := hex.EncodeToString(mac.Sum(nil))
			if hmac.Equal([]byte(sig), []byte(want)) {
				return id
			}
			return ""
		}
	}
	return ""
}

func newSessionID() string {
	b := make([]byte, 24)
	_, _ = rand.Read(b)
	return hex.EncodeToString(b)
}

// writeJSON is the single JSON-response path so we keep headers and
// error shape consistent across handlers.
func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(body)
}

func errorJSON(w http.ResponseWriter, status int, msg string) {
	writeJSON(w, status, map[string]string{"error": msg})
}
