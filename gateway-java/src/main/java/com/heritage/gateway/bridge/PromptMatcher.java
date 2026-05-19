package com.heritage.gateway.bridge;

import java.util.List;

/**
 * Single source of truth for the prompt and result strings emitted by
 * {@code bank_server}. Every literal here is copy-pasted from the
 * corresponding {@code write()} call in {@code bank_server/src/*.c}.
 *
 * <p>Match constants are loose substrings — the C server prefixes prompts
 * with newlines and varying punctuation, so {@code readUntil} relies on a
 * minimal stable substring being present.
 */
public final class PromptMatcher {

    private PromptMatcher() {}

    // ---------- main role-select menu ----------
    public static final String MAIN_MENU_CHOICE_PROMPT = "Enter your choice:";
    public static final String LOGIN_AS_MARKER = "===== Login As =====";

    // ---------- shared login ----------
    public static final String USERNAME_PROMPT = "Enter username:";
    public static final String PASSWORD_PROMPT = "Enter password:";

    // ---------- per-role menu markers ----------
    public static final String CUSTOMER_MENU_MARKER = "===== CUSTOMER MENU =====";
    public static final String EMPLOYEE_MENU_MARKER = "===== EMPLOYEE MENU =====";
    public static final String MANAGER_MENU_MARKER  = "===== MANAGER MENU =====";
    public static final String ADMIN_MENU_MARKER    = "===== ADMIN MENU =====";

    // ---------- post-login failure patterns ----------
    public static final String INVALID_CREDENTIALS = "Invalid credentials";
    public static final String ALREADY_LOGGED_IN   = "already logged in";

    // ---------- shared failure patterns ----------
    public static final String DEACTIVATED_KILL  = "deactivated by a manager";
    public static final String ROLE_CHANGED_KILL = "role has been changed";

    // ---------- customer prompts ----------
    public static final String DEPOSIT_PROMPT       = "Enter amount to deposit:";
    public static final String WITHDRAW_PROMPT     = "Enter amount to withdraw:";
    public static final String TRANSFER_TARGET_PROMPT = "Enter target username";
    public static final String TRANSFER_AMOUNT_PROMPT = "Enter amount to transfer:";
    public static final String LOAN_AMOUNT_PROMPT  = "Enter amount you wish to apply for:";
    public static final String FEEDBACK_PROMPT     = "Enter your feedback";
    public static final String NEW_PASSWORD_PROMPT     = "Enter new password:";
    public static final String CONFIRM_PASSWORD_PROMPT = "Confirm new password:";

    // ---------- customer result patterns ----------
    public static final String BALANCE_PREFIX        = "Your current balance is: $";
    public static final String DEPOSIT_OK_PREFIX     = "Deposited $";
    public static final String WITHDRAW_OK_PREFIX    = "Withdrew $";
    public static final String TRANSFER_OK_PREFIX    = "Transferred $";
    public static final String INSUFFICIENT_FUNDS    = "Insufficient funds";
    public static final String LOAN_SUBMITTED_PREFIX = "Loan application submitted";
    public static final String FEEDBACK_OK           = "Thank you! Your feedback has been submitted";
    public static final String TXN_HEADER            = "--- Your Transaction History ---";
    public static final String NO_TRANSACTIONS       = "No transactions found";
    public static final String PASSWORD_OK           = "Password updated successfully";
    public static final String INVALID_DEPOSIT       = "Invalid deposit amount";
    public static final String INVALID_WITHDRAW      = "Invalid withdrawal amount";
    public static final String INVALID_TRANSFER     = "Invalid transfer amount";
    public static final String SELF_TRANSFER         = "transfer money to your own account";
    public static final String TARGET_NOT_CUSTOMER   = "Target user not found or is not a customer";
    public static final String TARGET_DEACTIVATED    = "Target customer's";
    public static final String PASSWORDS_DO_NOT_MATCH = "Passwords do not match";

    // ---------- employee prompts ----------
    public static final String EMP_ADD_USERNAME_PROMPT = "Enter Customer Username:";
    public static final String EMP_ADD_PASSWORD_PROMPT = "Enter Password:";
    public static final String EMP_ADD_DEPOSIT_PROMPT  = "Enter Initial Deposit Amount:";
    public static final String EMP_MOD_USERNAME_PROMPT = "Enter customer username to modify:";
    public static final String EMP_MOD_NEW_NAME_PROMPT = "Enter new name";
    public static final String EMP_MOD_NEW_PASS_PROMPT = "Enter new password";
    public static final String EMP_LOAN_ID_PROMPT      = "Enter Loan ID to process:";
    public static final String EMP_LOAN_ACTION_PROMPT  = "Enter choice:";
    public static final String EMP_VIEW_TXN_USER_PROMPT = "Enter customer username to view transactions:";

    // ---------- employee result patterns ----------
    public static final String EMP_ADD_OK_PREFIX     = "Customer '";
    public static final String EMP_USER_ID_PREFIX    = "User ID:";
    public static final String EMP_ACCOUNT_ID_PREFIX = "Account ID:";
    public static final String EMP_MODIFY_OK         = "Customer login details modified successfully";
    public static final String EMP_LOAN_HEADER       = "--- Your Assigned PENDING Loans ---";
    public static final String EMP_LOAN_EMPTY        = "No pending loans assigned to you";
    public static final String EMP_LOAN_FOOTER       = "-------------------------------------";
    public static final String EMP_LOAN_DETAIL_HEADER = "-- Processing Loan ID";
    public static final String LOAN_APPROVED_OK      = "Loan APPROVED";
    public static final String LOAN_REJECTED_OK      = "Loan REJECTED";
    public static final String LOAN_NOT_ASSIGNED     = "not assigned to you";
    public static final String LOAN_NOT_FOUND        = "Loan ID not found";
    public static final String LOAN_ALREADY_PROCESSED = "already been";
    public static final String EMP_TXN_HEADER_PREFIX = "--- Transaction History for ";

    // ---------- manager prompts ----------
    public static final String MGR_TOGGLE_USER_PROMPT = "Enter customer username to toggle status:";
    public static final String MGR_ASSIGN_LOAN_ID_PROMPT = "Enter Loan ID to assign:";
    public static final String MGR_ASSIGN_EMP_PROMPT     = "Enter Employee username to assign to:";

    // ---------- manager result patterns ----------
    public static final String MGR_ACTIVATED   = "account activated successfully";
    public static final String MGR_DEACTIVATED = "account deactivated successfully";
    public static final String MGR_PENDING_HEADER = "--- Unassigned PENDING Loans ---";
    public static final String MGR_PENDING_FOOTER = "----------------------------------";
    public static final String MGR_NO_UNASSIGNED = "No unassigned pending loans found";
    public static final String MGR_ASSIGN_OK_PREFIX = "Loan ";
    public static final String MGR_FEEDBACK_HEADER = "--- Customer Feedback Log ---";
    public static final String MGR_FEEDBACK_EMPTY  = "No feedback has been submitted yet";
    public static final String MGR_LOAN_ALREADY_ASSIGNED = "is already assigned to employee";
    public static final String MGR_EMP_NOT_FOUND  = "Employee not found";

    // ---------- admin prompts ----------
    public static final String ADM_ADD_USERNAME_PROMPT = "Enter Employee Username:";
    public static final String ADM_ADD_ROLE_PROMPT     = "Enter choice (1-2):";
    public static final String ADM_ADD_PASSWORD_PROMPT = "Enter Password:";
    public static final String ADM_MOD_USERNAME_PROMPT = "Enter username to modify:";
    public static final String ADM_MOD_NEW_USER_PROMPT = "Enter new username";
    public static final String ADM_MOD_NEW_PASS_PROMPT = "Enter new password";
    public static final String ADM_ROLE_USERNAME_PROMPT = "Enter username to change role:";
    public static final String ADM_ROLE_CHOICE_PROMPT   = "Select new role";

    // ---------- admin result patterns ----------
    public static final String ADM_ADD_OK_PREFIX  = "Employee added successfully";
    public static final String ADM_MODIFY_OK      = "User details modified successfully";
    public static final String ADM_ROLE_OK_PREFIX = "Role updated:";
    public static final String ADM_USER_NOT_FOUND = "not found";
    public static final String ADM_INVALID_CHOICE = "Invalid";

    // ---------- generic ----------
    public static final List<String> ANY_PROMPT_AFTER_OPERATION = List.of(
            MAIN_MENU_CHOICE_PROMPT, CUSTOMER_MENU_MARKER, EMPLOYEE_MENU_MARKER,
            MANAGER_MENU_MARKER, ADMIN_MENU_MARKER, LOGIN_AS_MARKER);
}
