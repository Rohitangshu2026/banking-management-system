package com.heritage.gateway.bridge;

import com.heritage.gateway.config.BankProperties;
import com.heritage.gateway.dto.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.time.*;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static com.heritage.gateway.bridge.PromptMatcher.*;

/**
 * High-level operations against bank_server. Each method assumes the caller
 * holds {@code session.connection().lock()}. Long but mechanical — every
 * method is a small menu walk.
 *
 * <p>Pattern matching is intentionally tolerant: we look for the shortest
 * stable substring rather than anchoring on whole lines, because the
 * C server sometimes prefixes prompts with newlines or trailing menu text
 * we've already consumed.
 */
@Component
public class BankClient {

    private static final Logger log = LoggerFactory.getLogger(BankClient.class);
    private static final Duration STD_TIMEOUT = Duration.ofSeconds(8);

    /** [YYYY-MM-DD HH:MM] header that fronts every transaction history line. */
    private static final Pattern TXN_HEADER_RE = Pattern.compile(
            "\\[(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2})] ([A-Z]+):");

    /** Customer balance line: "Your current balance is: $123.45" */
    private static final Pattern BALANCE_RE = Pattern.compile("\\$([0-9]+\\.[0-9]{2})");

    /** Loan-application result line: "Loan application submitted. Your Loan ID is 5001" */
    private static final Pattern LOAN_ID_RE = Pattern.compile("Loan ID is (\\d+)");

    /** Employee add-customer result lines: "User ID: 16" / "Account ID: 1004". */
    private static final Pattern USER_ID_RE    = Pattern.compile("User ID:\\s*(\\d+)");
    private static final Pattern ACCOUNT_ID_RE = Pattern.compile("Account ID:\\s*(\\d+)");

    /** Per-line shape inside "View Assigned PENDING Loans". */
    private static final Pattern ASSIGNED_LOAN_RE = Pattern.compile(
            "Loan ID:\\s*(\\d+)\\s*\\|\\s*Customer:\\s*([^|]+?)\\s*\\(Acc:\\s*(\\d+)\\)\\s*\\|\\s*Amount:\\s*\\$([0-9]+\\.[0-9]{2})");

    /** Per-line shape inside "Unassigned PENDING Loans". */
    private static final Pattern UNASSIGNED_LOAN_RE = Pattern.compile(
            "Loan ID:\\s*(\\d+)\\s*\\|\\s*Account:\\s*(\\d+)\\s*\\|\\s*Amount:\\s*\\$([0-9]+\\.[0-9]{2})");

    /** Feedback header line: "[YYYY-MM-DD HH:MM] From: USERNAME (ID: 7)". */
    private static final Pattern FB_HEADER_RE = Pattern.compile(
            "\\[(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2})] From:\\s*(\\S+)\\s*\\(ID:\\s*(\\d+)\\)");

    /** "X.YY" inside a money string. */
    private static final Pattern MONEY_RE = Pattern.compile("\\$([0-9]+\\.[0-9]{2})");

    private static final DateTimeFormatter TXN_FMT =
            DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm", Locale.US);

    private final BankProperties props;

    public BankClient(BankProperties props) {
        this.props = props;
        log.info("BankClient configured for bank_server at {}:{} (connect={}ms, read={}ms)",
                props.host(), props.port(), props.connectTimeoutMs(), props.readTimeoutMs());
    }

    // ============================================================
    //  Connection lifecycle
    // ============================================================

    public BankConnection dial() {
        log.info("dialing bank_server at {}:{}", props.host(), props.port());
        try {
            BankConnection c = new BankConnection(props.host(), props.port(), props.connectTimeoutMs());
            log.info("dial OK to {}:{}", props.host(), props.port());
            return c;
        } catch (IOException e) {
            log.warn("dial FAILED to {}:{}: {}", props.host(), props.port(), e.toString());
            throw new BankProtocolException(502, "unable to reach bank_server at "
                    + props.host() + ":" + props.port(), e);
        }
    }

    /**
     * Drive the role-select → login → role-menu walk. On success returns
     * with the socket parked at the role menu's choice prompt.
     */
    public void authenticate(BankConnection c, String role, String username, String password) {
        int roleChoice = switch (role) {
            case "customer" -> 1;
            case "employee" -> 2;
            case "manager"  -> 3;
            case "admin"    -> 4;
            default -> throw new BankProtocolException(400, "unknown role: " + role);
        };
        String roleMarker = switch (role) {
            case "customer" -> CUSTOMER_MENU_MARKER;
            case "employee" -> EMPLOYEE_MENU_MARKER;
            case "manager"  -> MANAGER_MENU_MARKER;
            case "admin"    -> ADMIN_MENU_MARKER;
            default -> throw new BankProtocolException(400, "unknown role: " + role);
        };

        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        c.send(Integer.toString(roleChoice));

        c.readUntil(USERNAME_PROMPT, STD_TIMEOUT);
        c.send(username);

        c.readUntil(PASSWORD_PROMPT, STD_TIMEOUT);
        c.send(password);

        // Either the role menu marker shows up, or one of the rejection
        // patterns appears with the role-select menu re-displayed.
        String resp = c.readUntil(List.of(roleMarker, LOGIN_AS_MARKER), STD_TIMEOUT);
        if (resp.contains(INVALID_CREDENTIALS)) {
            throw new BankProtocolException(401, "invalid credentials");
        }
        if (resp.contains(ALREADY_LOGGED_IN)) {
            throw new BankProtocolException(409, "already logged in from another session");
        }
        if (!resp.contains(roleMarker)) {
            throw new BankProtocolException(502, "login response did not include " + roleMarker);
        }
        // Finally land at the role's choice prompt.
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
    }

    public void logout(BankSession session, int logoutChoice) {
        BankConnection c = session.connection();
        try {
            c.send(Integer.toString(logoutChoice));
            // Server prints "Logged out..." and returns to role-select. Best-effort.
            c.readUntil(List.of(LOGIN_AS_MARKER, MAIN_MENU_CHOICE_PROMPT), Duration.ofSeconds(2));
        } catch (BankProtocolException ignored) {
            // close path; the socket is going to be killed regardless.
        }
    }

    // ============================================================
    //  Customer operations
    // ============================================================

    public BalanceDto customerBalance(BankSession s) {
        BankConnection c = s.connection();
        c.send("1");
        String resp = c.readUntil(List.of(BALANCE_PREFIX, DEACTIVATED_KILL,
                MAIN_MENU_CHOICE_PROMPT), STD_TIMEOUT);
        guardDeactivated(resp);

        Matcher m = MONEY_RE.matcher(resp);
        if (!m.find()) {
            throw new BankProtocolException(502, "could not parse balance from: " + truncate(resp));
        }
        double balance = Double.parseDouble(m.group(1));
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return new BalanceDto(s.accountId() == null ? -1 : s.accountId(), balance);
    }

    public BalanceDto customerDeposit(BankSession s, double amount) {
        BankConnection c = s.connection();
        c.send("2");
        c.readUntil(List.of(DEPOSIT_PROMPT, DEACTIVATED_KILL), STD_TIMEOUT);
        c.send(formatAmount(amount));
        String resp = c.readUntil(List.of(DEPOSIT_OK_PREFIX, INVALID_DEPOSIT,
                "Error", "CRITICAL", DEACTIVATED_KILL), STD_TIMEOUT);
        guardDeactivated(resp);
        if (resp.contains(INVALID_DEPOSIT)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(422, "invalid deposit amount");
        }
        if (resp.contains("CRITICAL") || (resp.contains("Error") && !resp.contains(DEPOSIT_OK_PREFIX))) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(502, firstLine(resp));
        }
        double newBalance = parseNewBalance(resp);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return new BalanceDto(s.accountId() == null ? -1 : s.accountId(), newBalance);
    }

    public BalanceDto customerWithdraw(BankSession s, double amount) {
        BankConnection c = s.connection();
        c.send("3");
        c.readUntil(List.of(WITHDRAW_PROMPT, DEACTIVATED_KILL), STD_TIMEOUT);
        c.send(formatAmount(amount));
        String resp = c.readUntil(List.of(WITHDRAW_OK_PREFIX, INVALID_WITHDRAW,
                INSUFFICIENT_FUNDS, "Error", "CRITICAL", DEACTIVATED_KILL), STD_TIMEOUT);
        guardDeactivated(resp);
        if (resp.contains(INSUFFICIENT_FUNDS)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(422, "insufficient funds");
        }
        if (resp.contains(INVALID_WITHDRAW)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(422, "invalid withdrawal amount");
        }
        if (resp.contains("CRITICAL") || (resp.contains("Error") && !resp.contains(WITHDRAW_OK_PREFIX))) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(502, firstLine(resp));
        }
        double newBalance = parseNewBalance(resp);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return new BalanceDto(s.accountId() == null ? -1 : s.accountId(), newBalance);
    }

    public BalanceDto customerTransfer(BankSession s, String targetUsername, double amount) {
        BankConnection c = s.connection();
        c.send("4");
        c.readUntil(List.of(TRANSFER_TARGET_PROMPT, DEACTIVATED_KILL), STD_TIMEOUT);
        c.send(targetUsername);
        c.readUntil(List.of(TRANSFER_AMOUNT_PROMPT, TARGET_NOT_CUSTOMER, TARGET_DEACTIVATED,
                INVALID_TRANSFER), STD_TIMEOUT);
        // The amount prompt only appears if target lookup succeeded; if the
        // server is in an error path, send the amount anyway — the C server
        // is already returning to the menu, and we'll catch the error
        // pattern in the next read.
        c.send(formatAmount(amount));
        String resp = c.readUntil(List.of(TRANSFER_OK_PREFIX, INSUFFICIENT_FUNDS,
                SELF_TRANSFER, TARGET_NOT_CUSTOMER, TARGET_DEACTIVATED,
                INVALID_TRANSFER, "CRITICAL", MAIN_MENU_CHOICE_PROMPT), STD_TIMEOUT);
        guardDeactivated(resp);
        if (resp.contains(TARGET_NOT_CUSTOMER) || resp.contains(TARGET_DEACTIVATED)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(404, "target customer unavailable");
        }
        if (resp.contains(SELF_TRANSFER)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(422, "cannot transfer to your own account");
        }
        if (resp.contains(INSUFFICIENT_FUNDS)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(422, "insufficient funds");
        }
        if (resp.contains(INVALID_TRANSFER)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(422, "invalid transfer amount");
        }
        if (resp.contains("CRITICAL")) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(502, firstLine(resp));
        }
        double newBalance = parseNewBalance(resp);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return new BalanceDto(s.accountId() == null ? -1 : s.accountId(), newBalance);
    }

    public List<TransactionDto> customerTransactions(BankSession s) {
        BankConnection c = s.connection();
        c.send("5");
        String resp = c.readUntil(List.of(TXN_HEADER, NO_TRANSACTIONS,
                DEACTIVATED_KILL), STD_TIMEOUT);
        guardDeactivated(resp);
        // Then read until the menu re-appears so we have the full list.
        String body = c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return parseTransactions(resp + body);
    }

    public LoanCreatedDto customerApplyLoan(BankSession s, double amount) {
        BankConnection c = s.connection();
        c.send("6");
        c.readUntil(List.of(LOAN_AMOUNT_PROMPT, DEACTIVATED_KILL), STD_TIMEOUT);
        c.send(formatAmount(amount));
        String resp = c.readUntil(List.of(LOAN_SUBMITTED_PREFIX, "Invalid",
                "Failed", "Error"), STD_TIMEOUT);
        guardDeactivated(resp);
        Matcher m = LOAN_ID_RE.matcher(resp);
        if (!m.find()) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(422, firstLine(resp));
        }
        int loanId = Integer.parseInt(m.group(1));
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return new LoanCreatedDto(loanId, amount, "PENDING");
    }

    public void customerChangePassword(BankSession s, String newPassword) {
        changePasswordCommon(s, "7", newPassword);
    }

    public void customerFeedback(BankSession s, String message) {
        BankConnection c = s.connection();
        c.send("8");
        c.readUntil(List.of(FEEDBACK_PROMPT, DEACTIVATED_KILL), STD_TIMEOUT);
        c.send(message);
        String resp = c.readUntil(List.of(FEEDBACK_OK, "Error",
                DEACTIVATED_KILL), STD_TIMEOUT);
        guardDeactivated(resp);
        if (!resp.contains(FEEDBACK_OK)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(502, firstLine(resp));
        }
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
    }

    // ============================================================
    //  Employee operations
    // ============================================================

    public AddCustomerResultDto employeeAddCustomer(BankSession s, String username,
                                                   String password, double deposit) {
        BankConnection c = s.connection();
        c.send("1");
        c.readUntil(EMP_ADD_USERNAME_PROMPT, STD_TIMEOUT);
        c.send(username);
        c.readUntil(EMP_ADD_PASSWORD_PROMPT, STD_TIMEOUT);
        c.send(password);
        c.readUntil(EMP_ADD_DEPOSIT_PROMPT, STD_TIMEOUT);
        c.send(formatAmount(deposit));

        String resp = c.readUntil(List.of(EMP_ADD_OK_PREFIX, "Error"), STD_TIMEOUT);
        if (!resp.contains(EMP_ADD_OK_PREFIX)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(502, firstLine(resp));
        }
        // Trailing two lines: "User ID: N\nAccount ID: A\n".
        String tail = c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        String all = resp + tail;
        int userId = matchInt(USER_ID_RE, all);
        int accountId = matchInt(ACCOUNT_ID_RE, all);
        return new AddCustomerResultDto(userId, accountId, username);
    }

    public void employeeModifyCustomer(BankSession s, String targetUsername,
                                       String newName, String newPassword) {
        BankConnection c = s.connection();
        c.send("2");
        c.readUntil(EMP_MOD_USERNAME_PROMPT, STD_TIMEOUT);
        c.send(targetUsername);
        c.readUntil(EMP_MOD_NEW_NAME_PROMPT, STD_TIMEOUT);
        c.send(newName == null ? "" : newName);
        c.readUntil(EMP_MOD_NEW_PASS_PROMPT, STD_TIMEOUT);
        c.send(newPassword == null ? "" : newPassword);
        String resp = c.readUntil(List.of(EMP_MODIFY_OK, "Error", "not"), STD_TIMEOUT);
        if (!resp.contains(EMP_MODIFY_OK)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(404, firstLine(resp));
        }
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
    }

    public List<AssignedLoanDto> employeeAssignedLoans(BankSession s) {
        BankConnection c = s.connection();
        c.send("4");
        String header = c.readUntil(List.of(EMP_LOAN_HEADER, "No loans"), STD_TIMEOUT);
        if (header.contains("No loans")) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            return List.of();
        }
        String body = c.readUntil(EMP_LOAN_FOOTER, STD_TIMEOUT);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);

        List<AssignedLoanDto> out = new ArrayList<>();
        Matcher m = ASSIGNED_LOAN_RE.matcher(header + body);
        while (m.find()) {
            out.add(new AssignedLoanDto(
                    Integer.parseInt(m.group(1)),
                    m.group(2).trim(),
                    Integer.parseInt(m.group(3)),
                    Double.parseDouble(m.group(4)),
                    "PENDING"));
        }
        return out;
    }

    public String employeeProcessLoan(BankSession s, int loanId, String action) {
        BankConnection c = s.connection();
        c.send("3");
        c.readUntil(EMP_LOAN_ID_PROMPT, STD_TIMEOUT);
        c.send(Integer.toString(loanId));
        String pre = c.readUntil(List.of(EMP_LOAN_DETAIL_HEADER, LOAN_NOT_FOUND,
                LOAN_NOT_ASSIGNED, LOAN_ALREADY_PROCESSED, "Invalid", "Error"),
                STD_TIMEOUT);
        if (!pre.contains(EMP_LOAN_DETAIL_HEADER)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            if (pre.contains(LOAN_NOT_FOUND)) {
                throw new BankProtocolException(404, "loan not found");
            }
            if (pre.contains(LOAN_NOT_ASSIGNED)) {
                throw new BankProtocolException(403, "loan not assigned to you");
            }
            if (pre.contains(LOAN_ALREADY_PROCESSED)) {
                throw new BankProtocolException(409, "loan already processed");
            }
            throw new BankProtocolException(422, firstLine(pre));
        }
        c.readUntil(EMP_LOAN_ACTION_PROMPT, STD_TIMEOUT);
        int choice = switch (action) {
            case "approve" -> 1;
            case "reject"  -> 2;
            default -> throw new BankProtocolException(400, "action must be approve|reject");
        };
        c.send(Integer.toString(choice));
        String resp = c.readUntil(List.of(LOAN_APPROVED_OK, LOAN_REJECTED_OK,
                "CRITICAL", "Error"), STD_TIMEOUT);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        if (resp.contains(LOAN_APPROVED_OK)) return "APPROVED";
        if (resp.contains(LOAN_REJECTED_OK)) return "REJECTED";
        throw new BankProtocolException(502, firstLine(resp));
    }

    public List<TransactionDto> employeeCustomerTransactions(BankSession s, String username) {
        BankConnection c = s.connection();
        c.send("5");
        c.readUntil(EMP_VIEW_TXN_USER_PROMPT, STD_TIMEOUT);
        c.send(username);
        String resp = c.readUntil(List.of(EMP_TXN_HEADER_PREFIX, NO_TRANSACTIONS,
                "not found", "Error"), STD_TIMEOUT);
        if (resp.contains("not found")) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            throw new BankProtocolException(404, "customer not found");
        }
        String body = c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return parseTransactions(resp + body);
    }

    public void employeeChangePassword(BankSession s, String newPassword) {
        changePasswordCommon(s, "6", newPassword);
    }

    // ============================================================
    //  Manager operations
    // ============================================================

    public ToggleResultDto managerToggleCustomer(BankSession s, String username) {
        BankConnection c = s.connection();
        c.send("1");
        c.readUntil(MGR_TOGGLE_USER_PROMPT, STD_TIMEOUT);
        c.send(username);
        String resp = c.readUntil(List.of(MGR_ACTIVATED, MGR_DEACTIVATED,
                "not found", "not a customer", "Error", ROLE_CHANGED_KILL), STD_TIMEOUT);
        if (resp.contains(ROLE_CHANGED_KILL)) {
            throw new BankProtocolException(401, "your role has changed; please re-login");
        }
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        if (resp.contains(MGR_ACTIVATED))   return new ToggleResultDto(username, true);
        if (resp.contains(MGR_DEACTIVATED)) return new ToggleResultDto(username, false);
        throw new BankProtocolException(404, firstLine(resp));
    }

    public List<PendingLoanDto> managerPendingLoans(BankSession s) {
        BankConnection c = s.connection();
        c.send("2");
        String header = c.readUntil(List.of(MGR_PENDING_HEADER, "No loans"),
                STD_TIMEOUT);
        if (header.contains("No loans")) {
            // We may already be past the prompt; drain and re-issue 2 is not safe.
            // Server returns to main menu after this; consume.
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            return List.of();
        }
        String body = c.readUntil(MGR_PENDING_FOOTER, STD_TIMEOUT);
        // After printing the list the server reads two prompts (loan id, employee).
        // We don't have either to send, so send blanks to terminate the menu choice.
        String afterFooter = c.readUntil(List.of(MGR_ASSIGN_LOAN_ID_PROMPT,
                MGR_NO_UNASSIGNED, MAIN_MENU_CHOICE_PROMPT), STD_TIMEOUT);

        List<PendingLoanDto> out = new ArrayList<>();
        if (afterFooter.contains(MGR_NO_UNASSIGNED)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            return out;
        }
        if (afterFooter.contains(MGR_ASSIGN_LOAN_ID_PROMPT)) {
            // Send sentinel "0" so the server prints "Loan ID not found" and
            // returns to the menu without altering state.
            c.send("0");
            c.readUntil(List.of(MAIN_MENU_CHOICE_PROMPT, "not found"), STD_TIMEOUT);
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        }

        Matcher m = UNASSIGNED_LOAN_RE.matcher(header + body);
        while (m.find()) {
            out.add(new PendingLoanDto(
                    Integer.parseInt(m.group(1)),
                    Integer.parseInt(m.group(2)),
                    Double.parseDouble(m.group(3))));
        }
        return out;
    }

    public void managerAssignLoan(BankSession s, int loanId, String employeeUsername) {
        BankConnection c = s.connection();
        c.send("2");
        // Server prints the list first then prompts for IDs.
        c.readUntil(List.of(MGR_ASSIGN_LOAN_ID_PROMPT, MGR_NO_UNASSIGNED,
                "No loans"), STD_TIMEOUT);
        c.send(Integer.toString(loanId));
        c.readUntil(List.of(MGR_ASSIGN_EMP_PROMPT, LOAN_NOT_FOUND,
                MGR_LOAN_ALREADY_ASSIGNED, LOAN_ALREADY_PROCESSED, "Error"),
                STD_TIMEOUT);
        c.send(employeeUsername);
        String resp = c.readUntil(List.of(MGR_ASSIGN_OK_PREFIX + loanId + " successfully assigned",
                MGR_EMP_NOT_FOUND, MGR_LOAN_ALREADY_ASSIGNED, LOAN_NOT_FOUND,
                LOAN_ALREADY_PROCESSED, "Error"), STD_TIMEOUT);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        if (resp.contains(LOAN_NOT_FOUND)) {
            throw new BankProtocolException(404, "loan not found");
        }
        if (resp.contains(MGR_LOAN_ALREADY_ASSIGNED)) {
            throw new BankProtocolException(409, "loan already assigned");
        }
        if (resp.contains(LOAN_ALREADY_PROCESSED)) {
            throw new BankProtocolException(409, "loan already processed");
        }
        if (resp.contains(MGR_EMP_NOT_FOUND)) {
            throw new BankProtocolException(404, "employee not found or inactive");
        }
        if (!resp.contains("successfully assigned")) {
            throw new BankProtocolException(502, firstLine(resp));
        }
    }

    public List<FeedbackDto> managerFeedback(BankSession s) {
        BankConnection c = s.connection();
        c.send("3");
        String resp = c.readUntil(List.of(MGR_FEEDBACK_HEADER, MGR_FEEDBACK_EMPTY,
                ROLE_CHANGED_KILL, "Error"), STD_TIMEOUT);
        if (resp.contains(ROLE_CHANGED_KILL)) {
            throw new BankProtocolException(401, "your role has changed; please re-login");
        }
        if (resp.contains(MGR_FEEDBACK_EMPTY)) {
            c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
            return List.of();
        }
        String body = c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        return parseFeedback(resp + body);
    }

    public void managerChangePassword(BankSession s, String newPassword) {
        changePasswordCommon(s, "4", newPassword);
    }

    // ============================================================
    //  Admin operations
    // ============================================================

    public int adminAddUser(BankSession s, String username, String role, String password) {
        BankConnection c = s.connection();
        c.send("1");
        c.readUntil(ADM_ADD_USERNAME_PROMPT, STD_TIMEOUT);
        c.send(username);
        c.readUntil(ADM_ADD_ROLE_PROMPT, STD_TIMEOUT);
        int choice = switch (role) {
            case "manager"  -> 1;
            case "employee" -> 2;
            default -> throw new BankProtocolException(400, "role must be manager|employee");
        };
        c.send(Integer.toString(choice));
        c.readUntil(ADM_ADD_PASSWORD_PROMPT, STD_TIMEOUT);
        c.send(password);
        String resp = c.readUntil(List.of(ADM_ADD_OK_PREFIX, "Error", "Invalid"),
                STD_TIMEOUT);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        if (!resp.contains(ADM_ADD_OK_PREFIX)) {
            throw new BankProtocolException(502, firstLine(resp));
        }
        Matcher m = Pattern.compile("Employee ID:\\s*(\\d+)").matcher(resp);
        return m.find() ? Integer.parseInt(m.group(1)) : -1;
    }

    public void adminModifyUser(BankSession s, String targetUsername,
                                String newUsername, String newPassword) {
        BankConnection c = s.connection();
        c.send("2");
        c.readUntil(ADM_MOD_USERNAME_PROMPT, STD_TIMEOUT);
        c.send(targetUsername);
        c.readUntil(ADM_MOD_NEW_USER_PROMPT, STD_TIMEOUT);
        c.send(newUsername == null ? "" : newUsername);
        c.readUntil(ADM_MOD_NEW_PASS_PROMPT, STD_TIMEOUT);
        c.send(newPassword == null ? "" : newPassword);
        String resp = c.readUntil(List.of(ADM_MODIFY_OK, ADM_USER_NOT_FOUND,
                "Error"), STD_TIMEOUT);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        if (resp.contains(ADM_USER_NOT_FOUND) && !resp.contains(ADM_MODIFY_OK)) {
            throw new BankProtocolException(404, "user not found");
        }
        if (!resp.contains(ADM_MODIFY_OK)) {
            throw new BankProtocolException(502, firstLine(resp));
        }
    }

    public String adminChangeRole(BankSession s, String targetUsername, String newRole) {
        BankConnection c = s.connection();
        c.send("3");
        c.readUntil(ADM_ROLE_USERNAME_PROMPT, STD_TIMEOUT);
        c.send(targetUsername);
        c.readUntil(List.of(ADM_ROLE_CHOICE_PROMPT, ADM_USER_NOT_FOUND,
                "Cannot", "No change"), STD_TIMEOUT);
        int choice = switch (newRole) {
            case "employee" -> 1;
            case "manager"  -> 2;
            default -> throw new BankProtocolException(400, "role must be employee|manager");
        };
        c.send(Integer.toString(choice));
        String resp = c.readUntil(List.of(ADM_ROLE_OK_PREFIX, ADM_USER_NOT_FOUND,
                "Cannot", "No change", "Invalid", "Error"), STD_TIMEOUT);
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        if (resp.contains(ADM_USER_NOT_FOUND)) {
            throw new BankProtocolException(404, "user not found");
        }
        if (!resp.contains(ADM_ROLE_OK_PREFIX)) {
            throw new BankProtocolException(422, firstLine(resp));
        }
        return newRole;
    }

    public String adminLogs(BankSession s) {
        BankConnection c = s.connection();
        c.send("5");
        String resp = c.readUntil(MAIN_MENU_CHOICE_PROMPT, Duration.ofSeconds(10));
        // Strip the trailing menu the server re-printed.
        int marker = resp.indexOf(ADMIN_MENU_MARKER);
        return marker >= 0 ? resp.substring(0, marker).trim() : resp.trim();
    }

    public void adminChangePassword(BankSession s, String newPassword) {
        changePasswordCommon(s, "4", newPassword);
    }

    // ============================================================
    //  Shared helpers
    // ============================================================

    private void changePasswordCommon(BankSession s, String menuChoice, String newPassword) {
        BankConnection c = s.connection();
        c.send(menuChoice);
        c.readUntil(NEW_PASSWORD_PROMPT, STD_TIMEOUT);
        c.send(newPassword);
        c.readUntil(CONFIRM_PASSWORD_PROMPT, STD_TIMEOUT);
        c.send(newPassword);
        String resp = c.readUntil(List.of(PASSWORD_OK, PASSWORDS_DO_NOT_MATCH,
                "Error", "Failed", DEACTIVATED_KILL, ROLE_CHANGED_KILL), STD_TIMEOUT);
        guardDeactivated(resp);
        if (resp.contains(ROLE_CHANGED_KILL)) {
            throw new BankProtocolException(401, "your role has changed; please re-login");
        }
        c.readUntil(MAIN_MENU_CHOICE_PROMPT, STD_TIMEOUT);
        if (resp.contains(PASSWORDS_DO_NOT_MATCH)) {
            throw new BankProtocolException(422, "passwords do not match");
        }
        if (!resp.contains(PASSWORD_OK)) {
            throw new BankProtocolException(502, firstLine(resp));
        }
    }

    private static void guardDeactivated(String resp) {
        if (resp.contains(DEACTIVATED_KILL)) {
            throw new BankProtocolException(401, "your account has been deactivated");
        }
    }

    private static List<TransactionDto> parseTransactions(String text) {
        List<TransactionDto> out = new ArrayList<>();
        for (String raw : text.split("\n")) {
            String line = raw.trim();
            Matcher m = TXN_HEADER_RE.matcher(line);
            if (!m.find()) continue;

            String ts = m.group(1);
            String type = m.group(2);
            // The full money figure may not be the FIRST $X.XX on the line for
            // TRANSFER (which says "Sent $X.XX to Account Y" or "Received $X.XX
            // from Account Y"). Picking the first match works for both.
            Matcher mm = MONEY_RE.matcher(line);
            if (!mm.find()) continue;
            double amount = Double.parseDouble(mm.group(1));

            Integer counter = null;
            String memo = null;
            if ("TRANSFER".equals(type)) {
                Matcher cm = Pattern.compile("Account (\\d+)").matcher(line);
                if (cm.find()) counter = Integer.parseInt(cm.group(1));
                memo = line.contains("Sent") ? "Outgoing transfer" : "Incoming transfer";
                if (line.contains("Sent")) amount = -amount;
            } else if ("WITHDRAW".equals(type)) {
                amount = -amount;
                memo = "Withdrawal";
            } else if ("DEPOSIT".equals(type)) {
                memo = "Deposit";
            } else if ("LOAN".equals(type)) {
                memo = "Loan disbursal";
            }

            LocalDateTime ldt = LocalDateTime.parse(ts, TXN_FMT);
            Instant when = ldt.atZone(ZoneId.systemDefault()).toInstant();
            out.add(new TransactionDto(when, type, amount, counter, memo));
        }
        return out;
    }

    private static List<FeedbackDto> parseFeedback(String text) {
        List<FeedbackDto> out = new ArrayList<>();
        String[] lines = text.split("\n");
        for (int i = 0; i < lines.length; i++) {
            Matcher m = FB_HEADER_RE.matcher(lines[i]);
            if (!m.find()) continue;
            String ts = m.group(1);
            String username = m.group(2);
            int userId = Integer.parseInt(m.group(3));
            String message = "";
            if (i + 1 < lines.length && lines[i + 1].startsWith("Message:")) {
                message = lines[i + 1].substring("Message:".length()).trim();
            }
            LocalDateTime ldt = LocalDateTime.parse(ts, TXN_FMT);
            Instant when = ldt.atZone(ZoneId.systemDefault()).toInstant();
            out.add(new FeedbackDto(when, userId, username, message));
        }
        return out;
    }

    private static int matchInt(Pattern p, String text) {
        Matcher m = p.matcher(text);
        if (!m.find()) {
            throw new BankProtocolException(502, "could not parse integer from server output");
        }
        return Integer.parseInt(m.group(1));
    }

    /** Pull the "New balance: $X.YY" figure from the server's response line. */
    private static double parseNewBalance(String resp) {
        int idx = resp.indexOf("New balance: $");
        if (idx < 0) {
            // Fall back to last money figure on the line — works for the
            // "Your new balance is $X.YY" wording used by transfer.
            Matcher m = MONEY_RE.matcher(resp);
            double last = Double.NaN;
            while (m.find()) last = Double.parseDouble(m.group(1));
            if (Double.isNaN(last)) {
                throw new BankProtocolException(502, "could not parse new balance from: "
                        + truncate(resp));
            }
            return last;
        }
        Matcher m = BALANCE_RE.matcher(resp.substring(idx));
        if (!m.find()) {
            throw new BankProtocolException(502, "could not parse new balance from: "
                    + truncate(resp));
        }
        return Double.parseDouble(m.group(1));
    }

    private static String formatAmount(double amount) {
        return String.format(Locale.US, "%.2f", amount);
    }

    private static String firstLine(String resp) {
        for (String l : resp.split("\n")) {
            String t = l.trim();
            if (!t.isEmpty()) return t;
        }
        return resp.trim();
    }

    private static String truncate(String s) {
        String t = s.replace("\n", "\\n");
        return t.length() > 200 ? t.substring(t.length() - 200) : t;
    }
}
