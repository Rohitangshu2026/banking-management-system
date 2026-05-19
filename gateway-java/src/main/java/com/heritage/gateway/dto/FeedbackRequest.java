package com.heritage.gateway.dto;

import jakarta.validation.constraints.NotBlank;

public record FeedbackRequest(@NotBlank String message) {}
