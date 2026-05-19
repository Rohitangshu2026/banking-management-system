package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;

public record AddUserRequest(@NotBlank String username, @NotBlank String password,
                            @NotBlank String role) {}
