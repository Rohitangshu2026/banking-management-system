package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;

public record LoginRequest(@NotBlank String role, @NotBlank String username, @NotBlank String password) {}
