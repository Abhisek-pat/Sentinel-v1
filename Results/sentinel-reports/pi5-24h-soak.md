# Sentinel Pi 5 Soak-Test Analysis

Started: `2026-06-17T06:10:09.273868+00:00`
Finished: `2026-06-18T06:10:09.283645+00:00`
Result: **PASS**

## Summary

| Metric | Value |
|---|---:|
| Duration | 24.0 h |
| Availability | 100.0% |
| Samples | 4089 |
| Failures | 0 |
| Capture FPS avg/min | 14.95 / 14.1 |
| Detection FPS avg | 4.98 |
| Inference avg/max | 54.4 / 59.06 ms |
| Last frame age max | 93.0 ms |
| Temperature max | 55.6 C |
| Memory max | 23.2% |
| Disk max | 4.7% |
| RTSP reconnect delta | 0.0 |
| Throttled samples | 0 |
| Reasoning pending max | 206.0 |
| Reasoning backlog warning samples | 1 |
| Reasoning backlog max streak | 1 |

## Acceptance Gates

| Gate | Result |
|---|---:|
| availability_at_least_99_percent | PASS |
| capture_fps_average_at_least_12 | PASS |
| last_frame_age_below_1000_ms | PASS |
| temperature_below_75_c | PASS |
| no_throttling | PASS |
| no_rtsp_reconnect_growth | PASS |
| reasoning_backlog_not_sustained | PASS |
| llm_service_healthy | PASS |
| no_critical_dashboard_alerts | PASS |
