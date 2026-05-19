package com.heritage.gateway;

import com.heritage.gateway.config.BankProperties;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.CommandLineRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.scheduling.annotation.EnableScheduling;

@SpringBootApplication
@EnableConfigurationProperties(BankProperties.class)
@EnableScheduling
public class GatewayApplication {

    private static final Logger log = LoggerFactory.getLogger(GatewayApplication.class);

    public static void main(String[] args) {
        SpringApplication.run(GatewayApplication.class, args);
    }

    /** Print the resolved BankProperties at startup so we never have to guess. */
    @Bean
    CommandLineRunner dumpConfig(BankProperties props) {
        return args -> log.info(
                "[CONFIG] bank.host={}, bank.port={}, connect-timeout-ms={}, read-timeout-ms={}",
                props.host(), props.port(), props.connectTimeoutMs(), props.readTimeoutMs());
    }
}
