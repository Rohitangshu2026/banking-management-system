package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Positive;

public record AddCustomerRequest(@NotBlank String username, @NotBlank String password,
                                @NotNull @Positive Double initialDeposit) {}
