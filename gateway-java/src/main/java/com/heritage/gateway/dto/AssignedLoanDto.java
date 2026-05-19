package com.heritage.gateway.dto;

public record AssignedLoanDto(int loanId, String customerUsername, int accountId,
                             double amount, String status) {}
