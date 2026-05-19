package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;

public record ProcessLoanRequest(@NotBlank String action) {}
