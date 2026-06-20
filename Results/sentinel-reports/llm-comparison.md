# Sentinel LLM Model Comparison

Evaluation: `1781334723924-d1fdb852`
Label: `expanded-pi5-baseline`

## Ranking

| Rank | Provider | Model | Cases x Iterations | Success | Risk Accuracy | Avg Latency | P95 Latency |
|---:|---|---|---:|---:|---:|---:|---:|
| 1 | mock | sentinel-rules-v1 | 15 x 1 | 100.0% | 100.0% | 0.0 ms | 0.01 ms |
| 2 | openai-cloud | gpt-4.1-mini | 15 x 1 | 100.0% | 100.0% | 1338.12 ms | 2470.84 ms |

## Per-Risk Accuracy

| Provider | Low | Medium | High |
|---|---:|---:|---:|
| mock | 100.0 | 100.0 | 100.0 |
| openai-cloud | 100.0 | 100.0 | 100.0 |

## Recommendation

Use `mock` as the current comparison winner for this labeled suite. Re-run the report after adding local or LAN providers.
