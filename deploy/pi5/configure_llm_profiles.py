#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


def parse_profile(raw_profile: str) -> dict[str, object]:
    parts = raw_profile.split(",")
    values: dict[str, str] = {}
    for part in parts:
        if "=" not in part:
            raise ValueError(f"Profile segment must use key=value: {part}")
        key, value = part.split("=", 1)
        values[key.strip()] = value.strip()

    for required in ("name", "base_url", "model"):
        if not values.get(required):
            raise ValueError(f"Profile is missing required field: {required}")

    profile: dict[str, object] = {
        "name": values["name"],
        "type": values.get("type", "openai_compatible"),
        "base_url": values["base_url"].rstrip("/"),
        "model": values["model"],
        "timeout_sec": float(values.get("timeout_sec", "60")),
    }
    if values.get("api_key_env"):
        profile["api_key_env"] = values["api_key_env"]
    return profile


def render_env(
    profiles: list[dict[str, object]],
    default_provider: str,
    fallback_provider: str,
) -> str:
    if not profiles:
        raise ValueError("At least one profile is required.")
    names = {str(profile["name"]) for profile in profiles}
    if default_provider not in names and default_provider != "mock":
        raise ValueError(f"Default provider '{default_provider}' is not in the profile list.")
    if fallback_provider not in names and fallback_provider != "mock":
        raise ValueError(f"Fallback provider '{fallback_provider}' is not in the profile list.")
    profiles_json = json.dumps(profiles, separators=(",", ":"))
    return "\n".join(
        [
            f"SENTINEL_LLM_PROVIDER={default_provider}",
            f"SENTINEL_LLM_PROFILES_JSON='{profiles_json}'",
            f"SENTINEL_LLM_FALLBACK_PROVIDER={fallback_provider}",
            "SENTINEL_REASONING_POLL_SEC=1",
            "",
        ]
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render Sentinel LLM provider profiles for /etc/sentinel/llm.env."
    )
    parser.add_argument(
        "--profile",
        action="append",
        default=[],
        help=(
            "Provider as comma-separated key=value pairs. Required keys: "
            "name,base_url,model. Optional: api_key_env,timeout_sec,type."
        ),
    )
    parser.add_argument("--default-provider", default="mock")
    parser.add_argument("--fallback-provider", default="mock")
    parser.add_argument("--output", default="")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    profiles = [parse_profile(raw_profile) for raw_profile in args.profile]
    rendered = render_env(profiles, args.default_provider, args.fallback_provider)
    if args.output:
        output_path = Path(args.output)
        output_path.write_text(rendered, encoding="utf-8")
        print(f"Wrote {output_path}")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
