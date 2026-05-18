package main

import (
	"log"
	"net"
	"net/http"
	"sync"
	"time"
)

// withLogging emits one access log line per request. Keep it terse so it
// can be grepped easily and shipped to journald without ceremony.
func withLogging(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		sr := &statusRecorder{ResponseWriter: w, status: http.StatusOK}
		next.ServeHTTP(sr, r)
		log.Printf("%s %s %d %dms %s",
			r.Method, r.URL.Path, sr.status,
			time.Since(start).Milliseconds(), remoteIP(r))
	})
}

type statusRecorder struct {
	http.ResponseWriter
	status int
}

func (s *statusRecorder) WriteHeader(code int) {
	s.status = code
	s.ResponseWriter.WriteHeader(code)
}

// withRateLimit applies a coarse per-IP token bucket so a misbehaving
// client can't hammer /api/auth/login. The bank_server has no rate
// limiter of its own, so all of it lives here.
//
// The bucket is intentionally simple: 60 requests per minute per IP,
// reset by a background sweep every minute. For higher fidelity, swap
// in golang.org/x/time/rate; we avoid the dependency here to keep
// `go build` from needing the network on the user's machine.
func withRateLimit(next http.Handler) http.Handler {
	const limit = 60
	var (
		mu      sync.Mutex
		buckets = make(map[string]int)
	)
	go func() {
		t := time.NewTicker(time.Minute)
		for range t.C {
			mu.Lock()
			buckets = make(map[string]int, len(buckets))
			mu.Unlock()
		}
	}()
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		ip := remoteIP(r)
		mu.Lock()
		buckets[ip]++
		over := buckets[ip] > limit
		mu.Unlock()
		if over {
			errorJSON(w, http.StatusTooManyRequests, "slow down")
			return
		}
		next.ServeHTTP(w, r)
	})
}

func remoteIP(r *http.Request) string {
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		return r.RemoteAddr
	}
	return host
}
