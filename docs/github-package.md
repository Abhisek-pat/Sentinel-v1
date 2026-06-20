# GitHub-Ready Package

## Include

- `README.md` for the project overview and Pi 5 evidence summary.
- `docs/architecture.md` for runtime/data-flow architecture.
- `docs/pi5-deployment.md` for deployment and validation commands.
- `docs/portfolio.md` for interview-ready talking points.
- `docs/demo-script.md` for a short LinkedIn/GitHub walkthrough plan.
- `docs/edge-ai-architect-story.md` for the interview narrative.
- `docs/assets/sentinel-data-flow.png` and `.svg` for the architecture diagram.
- `deploy/pi5/run_gui_demo.sh` for showing the live OpenCV camera window on
  Raspberry Pi Desktop or VNC.
- `Results/sentinel-reports/` as the final evidence bundle.

## Evidence Bundle Contents

| File | Purpose |
|---|---|
| `diagnostics.txt` | Pi service, OS, thermal, API, and provider diagnostics |
| `pi5-24h-soak.json` | Raw 24-hour stability run |
| `pi5-24h-soak.md` | Human-readable 24-hour stability summary |
| `pi5-auth-smoke.json` | Authenticated dashboard smoke test |
| `evaluations.json` | Dashboard-exported model evaluation records |
| `llm-comparison.json` | Ranked LLM comparison data |
| `llm-comparison.md` | Human-readable LLM comparison summary |
| `sentinel-pi5-final-report.md` | Final one-page evidence report |
| `dashboard-final.png` | Final operations dashboard screenshot |

## Do Not Include

- `.venv/` or any local virtual environment.
- API keys or edited `/etc/sentinel/*.env` files containing secrets.
- Camera credentials or private RTSP URLs.
- Temporary evidence folders such as `Results/sentinel-reports_new/` or
  `Results/sentinel-reports_old/`.

## Final Publish Checklist

1. Confirm `git status --short` shows only intentional source, docs, and
   evidence changes.
2. Confirm `Results/sentinel-reports/sentinel-pi5-final-report.md` shows PASS
   for the 24-hour soak and dashboard auth smoke.
3. Add one dashboard screenshot to the README or release notes if desired.
4. Run the Python regression suite:

```powershell
.\.venv\Scripts\python.exe -B -m unittest `
  tests.test_dashboard_service `
  tests.test_soak_test `
  tests.test_analyze_soak `
  tests.test_analyze_evaluations `
  tests.test_configure_llm_profiles `
  tests.test_llm_service `
  tests.test_final_report
```

5. Commit with a message such as:

```text
Document Pi 5 validated deployment evidence
```

## Optional Follow-Up Evidence

If an INT8 model is added later, include:

- `detector-comparison.json`
- `detector-comparison.md`
- `pi5-int8-smoke.json`

Generate them with `deploy/pi5/analyze_detector_comparison.py` after running an
INT8 smoke soak.
