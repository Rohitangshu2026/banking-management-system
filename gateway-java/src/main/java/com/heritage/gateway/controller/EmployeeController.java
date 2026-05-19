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
public class EmployeeController {

    private final BankClient bank;
    private final SessionGuard guard;

    public EmployeeController(BankClient bank, SessionGuard guard) {
        this.bank = bank;
        this.guard = guard;
    }

    @PostMapping("/customers")
    public AddCustomerResultDto addCustomer(@Valid @RequestBody AddCustomerRequest body,
                                            HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "employee");
        synchronized (s.connection().lock()) {
            return bank.employeeAddCustomer(s, body.username(), body.password(), body.initialDeposit());
        }
    }

    @PutMapping("/customers/{username}")
    public java.util.Map<String, String> modifyCustomer(@PathVariable String username,
                                                        @RequestBody ModifyCustomerRequest body,
                                                        HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "employee");
        synchronized (s.connection().lock()) {
            bank.employeeModifyCustomer(s, username, body.newName(), body.newPassword());
        }
        return java.util.Map.of("status", "ok");
    }

    @GetMapping("/loans/assigned")
    public List<AssignedLoanDto> assignedLoans(HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "employee");
        synchronized (s.connection().lock()) {
            return bank.employeeAssignedLoans(s);
        }
    }

    @PostMapping("/loans/{id}/process")
    public ProcessLoanResultDto processLoan(@PathVariable("id") int loanId,
                                            @Valid @RequestBody ProcessLoanRequest body,
                                            HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "employee");
        synchronized (s.connection().lock()) {
            String status = bank.employeeProcessLoan(s, loanId, body.action());
            return new ProcessLoanResultDto(loanId, status);
        }
    }

    @GetMapping("/customers/{username}/transactions")
    public List<TransactionDto> customerTransactions(@PathVariable String username,
                                                     HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "employee");
        synchronized (s.connection().lock()) {
            return bank.employeeCustomerTransactions(s, username);
        }
    }
}
