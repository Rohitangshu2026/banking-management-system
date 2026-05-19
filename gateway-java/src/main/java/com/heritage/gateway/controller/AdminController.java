package com.heritage.gateway.controller;

import com.heritage.gateway.bridge.BankClient;
import com.heritage.gateway.bridge.BankSession;
import com.heritage.gateway.dto.*;
import com.heritage.gateway.web.SessionGuard;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api")
public class AdminController {

    private final BankClient bank;
    private final SessionGuard guard;

    public AdminController(BankClient bank, SessionGuard guard) {
        this.bank = bank;
        this.guard = guard;
    }

    @PostMapping("/users")
    public AddUserResultDto addUser(@Valid @RequestBody AddUserRequest body,
                                    HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "admin");
        int id;
        synchronized (s.connection().lock()) {
            id = bank.adminAddUser(s, body.username(), body.role(), body.password());
        }
        return new AddUserResultDto(body.username(), body.role(), id);
    }

    @PutMapping("/users/{username}")
    public java.util.Map<String, String> modifyUser(@PathVariable String username,
                                                    @RequestBody ModifyUserRequest body,
                                                    HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "admin");
        synchronized (s.connection().lock()) {
            bank.adminModifyUser(s, username, body.newUsername(), body.newPassword());
        }
        return java.util.Map.of("status", "ok");
    }

    @PatchMapping("/users/{username}/role")
    public ChangeRoleResultDto changeRole(@PathVariable String username,
                                          @Valid @RequestBody ChangeRoleRequest body,
                                          HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "admin");
        synchronized (s.connection().lock()) {
            String role = bank.adminChangeRole(s, username, body.newRole());
            return new ChangeRoleResultDto(username, role);
        }
    }

    @GetMapping("/logs")
    public LogsDto logs(HttpServletRequest req) {
        BankSession s = guard.requireRole(req, "admin");
        synchronized (s.connection().lock()) {
            return new LogsDto(bank.adminLogs(s));
        }
    }
}
