package com.heritage.gateway.controller;

import com.heritage.gateway.bridge.BankClient;
import com.heritage.gateway.bridge.BankSession;
import com.heritage.gateway.dto.*;
import com.heritage.gateway.web.SessionGuard;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api")
public class CustomerController {

    private final BankClient bank;
    private final SessionGuard guard;

    public CustomerController(BankClient bank, SessionGuard guard) {
        this.bank = bank;
        this.guard = guard;
    }

    @GetMapping("/account")
    public BalanceDto balance(HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "customer");
        synchronized (s.connection().lock()) {
            return bank.customerBalance(s);
        }
    }

    @GetMapping("/transactions")
    public List<TransactionDto> transactions(HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "customer");
        synchronized (s.connection().lock()) {
            return bank.customerTransactions(s);
        }
    }

    @PostMapping("/deposit")
    public BalanceDto deposit(@Valid @RequestBody AmountRequest body,
                              HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "customer");
        synchronized (s.connection().lock()) {
            return bank.customerDeposit(s, body.amount());
        }
    }

    @PostMapping("/withdraw")
    public BalanceDto withdraw(@Valid @RequestBody AmountRequest body,
                               HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "customer");
        synchronized (s.connection().lock()) {
            return bank.customerWithdraw(s, body.amount());
        }
    }

    @PostMapping("/transfer")
    public BalanceDto transfer(@Valid @RequestBody TransferRequest body,
                               HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "customer");
        synchronized (s.connection().lock()) {
            return bank.customerTransfer(s, body.targetUsername(), body.amount());
        }
    }

    @PostMapping("/loans")
    public LoanCreatedDto applyForLoan(@Valid @RequestBody LoanApplyRequest body,
                                       HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "customer");
        synchronized (s.connection().lock()) {
            return bank.customerApplyLoan(s, body.amount());
        }
    }

    @PostMapping("/feedback")
    public java.util.Map<String, String> feedback(@Valid @RequestBody FeedbackRequest body,
                                                  HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "customer");
        synchronized (s.connection().lock()) {
            bank.customerFeedback(s, body.message());
        }
        return java.util.Map.of("status", "ok");
    }

    @PostMapping("/password")
    public java.util.Map<String, Object> changePassword(@Valid @RequestBody ChangePasswordRequest body,
                                                        HttpServletRequest req) {
        BankSession s = guard.require(req);
        synchronized (s.connection().lock()) {
            switch (s.role()) {
                case "customer" -> bank.customerChangePassword(s, body.newPassword());
                case "employee" -> bank.employeeChangePassword(s, body.newPassword());
                case "manager"  -> bank.managerChangePassword(s, body.newPassword());
                case "admin"    -> bank.adminChangePassword(s, body.newPassword());
                default -> throw new IllegalStateException("unknown role");
            }
        }
        return java.util.Map.of("status", "ok", "reLogin", false);
    }
}
