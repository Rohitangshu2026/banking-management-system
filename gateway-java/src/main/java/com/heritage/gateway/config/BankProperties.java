package com.heritage.gateway.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * Connection settings for the upstream {@code bank_server}. Read from the
 * {@code bank.*} keys in application.yml or matching {@code BANK_*}
 * environment variables.
 */
@ConfigurationProperties("bank")
public record BankProperties(
        String host,
        int port,
        int connectTimeoutMs,
        int readTimeoutMs) {
}
