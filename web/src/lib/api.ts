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
  accountId: number | null;
}

export interface Balance {
  accountId: number;
  balance: number;
}

export interface Transaction {
  ts: string; // ISO instant from the gateway
  type: "DEPOSIT" | "WITHDRAW" | "TRANSFER" | "LOAN";
  amount: number;
  counterpartyAccount: number | null;
  memo: string | null;
}

export interface LoanCreated {
  loanId: number;
  amount: number;
  status: string;
}

export interface AddCustomerResult {
  userId: number;
  accountId: number;
  username: string;
}

export interface AssignedLoan {
  loanId: number;
  customerUsername: string;
  accountId: number;
  amount: number;
  status: string;
}

export interface PendingLoan {
  loanId: number;
  accountId: number;
  amount: number;
}

export interface ToggleResult {
  username: string;
  isActive: boolean;
}

export interface Feedback {
  ts: string;
  userId: number;
  username: string;
  message: string;
}

export interface AddUserResult {
  username: string;
  role: string;
  employeeId: number;
}

export interface ChangeRoleResult {
  username: string;
  newRole: string;
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

  // auth
  login: (role: Role, username: string, password: string) =>
    request<Me>("POST", "/api/auth/login", { role, username, password }),
  logout: () => request<{ status: string }>("POST", "/api/auth/logout"),
  me: () => request<Me>("GET", "/api/me"),

  // customer
  account: () => request<Balance>("GET", "/api/account"),
  transactions: () => request<Transaction[]>("GET", "/api/transactions"),
  deposit: (amount: number) => request<Balance>("POST", "/api/deposit", { amount }),
  withdraw: (amount: number) => request<Balance>("POST", "/api/withdraw", { amount }),
  transfer: (targetUsername: string, amount: number) =>
    request<Balance>("POST", "/api/transfer", { targetUsername, amount }),
  applyLoan: (amount: number) =>
    request<LoanCreated>("POST", "/api/loans", { amount }),
  feedback: (message: string) =>
    request<{ status: string }>("POST", "/api/feedback", { message }),
  changePassword: (newPassword: string) =>
    request<{ status: string }>("POST", "/api/password", { newPassword }),

  // employee
  addCustomer: (username: string, password: string, initialDeposit: number) =>
    request<AddCustomerResult>("POST", "/api/customers", {
      username,
      password,
      initialDeposit,
    }),
  modifyCustomer: (username: string, newName?: string, newPassword?: string) =>
    request<{ status: string }>("PUT", `/api/customers/${encodeURIComponent(username)}`, {
      newName,
      newPassword,
    }),
  assignedLoans: () => request<AssignedLoan[]>("GET", "/api/loans/assigned"),
  processLoan: (loanId: number, action: "approve" | "reject") =>
    request<{ loanId: number; status: string }>(
      "POST",
      `/api/loans/${loanId}/process`,
      { action }
    ),
  customerTransactions: (username: string) =>
    request<Transaction[]>(
      "GET",
      `/api/customers/${encodeURIComponent(username)}/transactions`
    ),

  // manager
  pendingLoans: () => request<PendingLoan[]>("GET", "/api/loans/pending"),
  assignLoan: (loanId: number, employeeUsername: string) =>
    request<{ status: string; loanId: number; assignedTo: string }>(
      "POST",
      `/api/loans/${loanId}/assign`,
      { employeeUsername }
    ),
  toggleCustomer: (username: string) =>
    request<ToggleResult>(
      "POST",
      `/api/customers/${encodeURIComponent(username)}/toggle`
    ),
  feedbackInbox: () => request<Feedback[]>("GET", "/api/feedback"),

  // admin
  addUser: (username: string, password: string, role: "employee" | "manager") =>
    request<AddUserResult>("POST", "/api/users", { username, password, role }),
  modifyUser: (username: string, newUsername?: string, newPassword?: string) =>
    request<{ status: string }>(
      "PUT",
      `/api/users/${encodeURIComponent(username)}`,
      { newUsername, newPassword }
    ),
  changeRole: (username: string, newRole: "employee" | "manager") =>
    request<ChangeRoleResult>(
      "PATCH",
      `/api/users/${encodeURIComponent(username)}/role`,
      { newRole }
    ),
  logs: () => request<{ text: string }>("GET", "/api/logs"),
};
