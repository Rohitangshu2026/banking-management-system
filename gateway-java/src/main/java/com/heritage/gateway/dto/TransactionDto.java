package com.heritage.gateway.dto;

import java.time.Instant;

public record TransactionDto(Instant ts, String type, double amount, Integer counterpartyAccount, String memo) {}
