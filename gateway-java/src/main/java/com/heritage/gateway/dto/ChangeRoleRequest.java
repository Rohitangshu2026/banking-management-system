package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;

public record ChangeRoleRequest(@NotBlank String newRole) {}
