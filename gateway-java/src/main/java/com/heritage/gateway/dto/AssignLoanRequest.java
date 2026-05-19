package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;

public record AssignLoanRequest(@NotBlank String employeeUsername) {}
