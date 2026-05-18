package main

import (
	"bufio"
	"errors"
	"fmt"
	"net"
	"strings"
	"sync"
	"time"
)

// bankConn wraps the TCP connection to bank_server and provides a small
// helper API for driving its menu state machine.
//
// The protocol the bank server speaks is line-oriented but not strict:
// menus arrive as multi-line blocks ending with a prompt like
// "Enter your choice:" or "Enter username: ", and the server reads input
// up to the next newline. We mirror that here — readUntil() returns
// everything up to and including the trailing prompt line, and send()
// writes the input with the expected '\n' terminator.
type bankConn struct {
	c  net.Conn
	br *bufio.Reader

	// One bridge serializes all menu transitions. Two concurrent HTTP
	// handlers must not race on the same TCP connection.
	mu sync.Mutex
}

func dialBank(addr string) (*bankConn, error) {
	c, err := net.DialTimeout("tcp", addr, 5*time.Second)
	if err != nil {
		return nil, err
	}
	return &bankConn{
		c:  c,
		br: bufio.NewReader(c),
	}, nil
}

func (b *bankConn) Close() error {
	if b == nil || b.c == nil {
		return nil
	}
	return b.c.Close()
}

// readUntil consumes bytes from the bank server until it sees one of the
// listed prompt substrings or hits the deadline. It returns the full
// accumulated text so callers can pattern-match on errors emitted by
// the server (e.g. "Invalid credentials").
func (b *bankConn) readUntil(prompts []string, timeout time.Duration) (string, error) {
	deadline := time.Now().Add(timeout)
	if err := b.c.SetReadDeadline(deadline); err != nil {
		return "", err
	}
	defer b.c.SetReadDeadline(time.Time{})

	var buf strings.Builder
	chunk := make([]byte, 512)
	for {
		n, err := b.br.Read(chunk)
		if n > 0 {
			buf.Write(chunk[:n])
			s := buf.String()
			for _, p := range prompts {
				if strings.Contains(s, p) {
					return s, nil
				}
			}
		}
		if err != nil {
			return buf.String(), err
		}
	}
}

func (b *bankConn) send(line string) error {
	_, err := fmt.Fprintf(b.c, "%s\n", line)
	return err
}

// authenticate drives the bank server through:
//
//	main menu -> role select -> username -> password
//
// and returns nil on success. The bank server has no machine-readable
// success indicator; it just transitions to the next role-specific menu.
// We detect that by waiting for the next menu's first prompt line.
func (b *bankConn) authenticate(role, username, password string) error {
	b.mu.Lock()
	defer b.mu.Unlock()

	roleChoice, ok := map[string]string{
		"customer": "1",
		"employee": "2",
		"manager":  "3",
		"admin":    "4",
	}[role]
	if !ok {
		return errors.New("unknown role")
	}

	// Wait for the main menu prompt.
	if _, err := b.readUntil([]string{"Enter your choice:"}, 5*time.Second); err != nil {
		return fmt.Errorf("main menu: %w", err)
	}
	if err := b.send(roleChoice); err != nil {
		return err
	}

	if _, err := b.readUntil([]string{"Enter username"}, 5*time.Second); err != nil {
		return fmt.Errorf("username prompt: %w", err)
	}
	if err := b.send(username); err != nil {
		return err
	}

	if _, err := b.readUntil([]string{"Enter password"}, 5*time.Second); err != nil {
		return fmt.Errorf("password prompt: %w", err)
	}
	if err := b.send(password); err != nil {
		return err
	}

	// Either we land on the role's menu (any "Enter your choice" prompt)
	// or the server emits a failure line and bounces us back. We treat
	// "Invalid" or "deactivated" anywhere in the buffer as failure.
	resp, err := b.readUntil([]string{"Enter your choice"}, 5*time.Second)
	if err != nil {
		return fmt.Errorf("post-login: %w", err)
	}
	lower := strings.ToLower(resp)
	if strings.Contains(lower, "invalid") || strings.Contains(lower, "deactivated") {
		return errors.New("authentication failed")
	}
	return nil
}
