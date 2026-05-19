package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Positive;

public record TransferRequest(@NotBlank String targetUsername, @NotNull @Positive Double amount) {}
