package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Positive;

public record LoanApplyRequest(@NotNull @Positive Double amount) {}
