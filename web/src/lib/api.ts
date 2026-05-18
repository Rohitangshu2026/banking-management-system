/**
 * Thin wrapper around fetch that:
 *   - sends/receives JSON
 *   - includes the session cookie on every request
 *   - normalises error responses into a thrown ApiError
 *
 * Keep this file the only thing in the codebase that knows about the
 * gateway URL shape. Everything else imports the typed helpers.
 */

export class ApiError extends Error {
  constructor(public status: number, public body: unknown, message?: string) {
    super(message ?? `request failed: ${status}`);
  }
}

export type Role = "customer" | "employee" | "manager" | "admin";

export interface Me {
  role: Role;
  username: string;
}

async function request<T>(
  method: string,
  path: string,
  body?: unknown
): Promise<T> {
  const res = await fetch(path, {
    method,
    credentials: "include",
    headers: body ? { "Content-Type": "application/json" } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });

  const text = await res.text();
  const parsed = text ? safeJSON(text) : null;

  if (!res.ok) {
    const msg =
      (parsed && typeof parsed === "object" && "error" in parsed
        ? String((parsed as { error: unknown }).error)
        : null) ?? res.statusText;
    throw new ApiError(res.status, parsed, msg);
  }
  return parsed as T;
}

function safeJSON(s: string): unknown {
  try {
    return JSON.parse(s);
  } catch {
    return s;
  }
}

export const api = {
  health: () => request<{ status: string }>("GET", "/api/health"),

  login: (role: Role, username: string, password: string) =>
    request<Me>("POST", "/api/auth/login", { role, username, password }),

  logout: () => request<{ status: string }>("POST", "/api/auth/logout"),

  me: () => request<Me>("GET", "/api/me"),
};
