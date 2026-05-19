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
public class ManagerController {

    private final BankClient bank;
    private final SessionGuard guard;

    public ManagerController(BankClient bank, SessionGuard guard) {
        this.bank = bank;
        this.guard = guard;
    }

    @PostMapping("/customers/{username}/toggle")
    public ToggleResultDto toggleCustomer(@PathVariable String username, HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "manager");
        synchronized (s.connection().lock()) {
            return bank.managerToggleCustomer(s, username);
        }
    }

    @GetMapping("/loans/pending")
    public List<PendingLoanDto> pendingLoans(HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "manager");
        synchronized (s.connection().lock()) {
            return bank.managerPendingLoans(s);
        }
    }

    @PostMapping("/loans/{id}/assign")
    public java.util.Map<String, Object> assignLoan(@PathVariable("id") int loanId,
                                                    @Valid @RequestBody AssignLoanRequest body,
                                                    HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "manager");
        synchronized (s.connection().lock()) {
            bank.managerAssignLoan(s, loanId, body.employeeUsername());
        }
        return java.util.Map.of("status", "ok", "loanId", loanId,
                "assignedTo", body.employeeUsername());
    }

    @GetMapping("/feedback")
    public List<FeedbackDto> feedback(HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "manager");
        synchronized (s.connection().lock()) {
            return bank.managerFeedback(s);
        }
    }
}
