package com.heritage.gateway.dto;

import java.time.Instant;

public record FeedbackDto(Instant ts, int userId, String username, String message) {}
