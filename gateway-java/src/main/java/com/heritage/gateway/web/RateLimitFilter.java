package com.heritage.gateway.web;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Coarse per-IP token bucket. The C bank_server has no rate limiter of
 * its own, so this is the only thing between a noisy client and a
 * thundering herd of menu walks.
 *
 * <p>One bucket per remote address, {@code rate-limit-per-minute}
 * tokens, reset on the minute boundary. Replaces the bucket the Go
 * gateway had — same shape, no external dependency.
 *
 * <p>Higher-fidelity rate limiting (per-route, distributed, weighted)
 * is what bucket4j is for; we'd want it before exposing this gateway
 * to the open internet. For demo / single-host operation this is
 * enough.
 */
@Component
@Order(Ordered.HIGHEST_PRECEDENCE)
public class RateLimitFilter extends OncePerRequestFilter {

    private final int limitPerMinute;
    private final Map<String, AtomicInteger> buckets = new ConcurrentHashMap<>();
    private final AtomicLong resetEpochMin = new AtomicLong(epochMinute());

    public RateLimitFilter(@Value("${gateway.rate-limit-per-minute:120}") int limitPerMinute) {
        this.limitPerMinute = limitPerMinute;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest req, HttpServletResponse res,
                                    FilterChain chain) throws ServletException, IOException {
        if (!req.getRequestURI().startsWith("/api/")) {
            chain.doFilter(req, res);
            return;
        }

        // Sweep when the minute rolls over. Single CAS owner does the
        // reset; everyone else sees the cleared map.
        long now = epochMinute();
        long prev = resetEpochMin.get();
        if (now > prev && resetEpochMin.compareAndSet(prev, now)) {
            buckets.clear();
        }

        String ip = req.getRemoteAddr();
        int count = buckets.computeIfAbsent(ip, k -> new AtomicInteger()).incrementAndGet();
        if (count > limitPerMinute) {
            res.setStatus(429);
            res.setHeader("Retry-After", "60");
            res.setContentType("application/json;charset=utf-8");
            res.getWriter().write("{\"error\":\"slow down\"}");
            return;
        }
        chain.doFilter(req, res);
    }

    private static long epochMinute() {
        return System.currentTimeMillis() / 60_000L;
    }
}
