import json
import os
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from typing import Any, Protocol


class ProviderError(RuntimeError):
    pass


@dataclass(frozen=True)
class ReasoningResult:
    summary: str
    risk_level: str
    recommended_action: str
    provider: str
    model: str


class ReasoningProvider(Protocol):
    model: str

    def configuration_error(self) -> str:
        ...

    def available(self) -> bool:
        ...

    def reason(self, scene_state: dict[str, Any]) -> ReasoningResult:
        ...


class MockProvider:
    model = "sentinel-rules-v1"

    def configuration_error(self) -> str:
        return ""

    def available(self) -> bool:
        return True

    def reason(self, scene_state: dict[str, Any]) -> ReasoningResult:
        persons = scene_state.get("persons", [])
        events = [str(event).lower() for event in scene_state.get("recent_events", [])]
        loitering = any(bool(person.get("loitering")) for person in persons)
        high_risk = any(
            marker in event
            for event in events
            for marker in ("forced entry", "weapon", "intrusion", "emergency")
        )

        if high_risk:
            return self._result(
                "High-risk activity reported in the monitored scene.",
                "high",
                "Notify an operator and review the latest event clip.",
            )
        if loitering:
            return self._result(
                "A person is loitering in a monitored zone.",
                "medium",
                "Review the active track and saved event clip.",
            )
        if persons:
            return self._result(
                f"{len(persons)} active person track(s) detected.",
                "low",
                "Continue monitoring.",
            )
        return self._result(
            "No active person detected.",
            "low",
            "Continue monitoring.",
        )

    def _result(self, summary: str, risk: str, action: str) -> ReasoningResult:
        return ReasoningResult(
            summary=summary,
            risk_level=risk,
            recommended_action=action,
            provider="mock",
            model=self.model,
        )


class OpenAiCompatibleProvider:
    def __init__(
        self,
        name: str,
        base_url: str,
        model: str,
        api_key_env: str = "",
        timeout_sec: float = 30.0,
    ) -> None:
        self.name = name
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.api_key_env = api_key_env
        self.timeout_sec = timeout_sec

    def configuration_error(self) -> str:
        if not self.base_url:
            return "base_url is missing."
        if not self.model:
            return "model is missing."

        parsed_url = urllib.parse.urlparse(self.base_url)
        if parsed_url.scheme not in {"http", "https"} or not parsed_url.netloc:
            return "base_url must be an HTTP or HTTPS endpoint."

        placeholder_values = (
            self.base_url.lower(),
            self.model.lower(),
        )
        placeholder_markers = (
            "model-server",
            "model-name",
            "replace-me",
            "example.invalid",
        )
        if any(marker in value for marker in placeholder_markers for value in placeholder_values):
            return "profile still contains documentation placeholder values."

        if self.api_key_env and not os.environ.get(self.api_key_env):
            return f"required credential environment variable '{self.api_key_env}' is not set."
        return ""

    def available(self) -> bool:
        return not self.configuration_error()

    def reason(self, scene_state: dict[str, Any]) -> ReasoningResult:
        if not self.available():
            raise ProviderError(
                f"Provider '{self.name}' is unavailable: {self.configuration_error()}"
            )

        request_body = json.dumps(
            {
                "model": self.model,
                "temperature": 0,
                "messages": [
                    {
                        "role": "system",
                        "content": (
                            "You are Sentinel's surveillance reasoning engine. "
                            "Use only the structured scene state. Return strict JSON "
                            "with summary, risk_level, and recommended_action. "
                            "risk_level must be low, medium, or high."
                        ),
                    },
                    {
                        "role": "user",
                        "content": json.dumps(scene_state, separators=(",", ":")),
                    },
                ],
            }
        ).encode("utf-8")
        headers = {"Content-Type": "application/json"}
        if self.api_key_env:
            headers["Authorization"] = f"Bearer {os.environ[self.api_key_env]}"

        request = urllib.request.Request(
            f"{self.base_url}/chat/completions",
            data=request_body,
            headers=headers,
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=self.timeout_sec) as response:
                response_body = response.read().decode("utf-8")
        except (urllib.error.URLError, TimeoutError, OSError) as error:
            raise ProviderError(f"Provider '{self.name}' request failed: {error}") from error

        try:
            payload = json.loads(response_body)
            content = payload["choices"][0]["message"]["content"].strip()
            if content.startswith("```"):
                content = content.removeprefix("```json").removeprefix("```")
                content = content.removesuffix("```").strip()
            reasoning = json.loads(content)
            summary = str(reasoning["summary"]).strip()
            risk_level = str(reasoning["risk_level"]).strip().lower()
            recommended_action = str(reasoning["recommended_action"]).strip()
        except (json.JSONDecodeError, KeyError, IndexError, TypeError) as error:
            raise ProviderError(
                f"Provider '{self.name}' returned an invalid reasoning response."
            ) from error

        if not summary or not recommended_action or risk_level not in {"low", "medium", "high"}:
            raise ProviderError(f"Provider '{self.name}' returned invalid reasoning fields.")

        return ReasoningResult(
            summary=summary,
            risk_level=risk_level,
            recommended_action=recommended_action,
            provider=self.name,
            model=self.model,
        )


def create_providers() -> dict[str, ReasoningProvider]:
    providers: dict[str, ReasoningProvider] = {"mock": MockProvider()}
    profiles_json = os.environ.get("SENTINEL_LLM_PROFILES_JSON", "[]")
    try:
        profiles = json.loads(profiles_json)
    except json.JSONDecodeError as error:
        raise RuntimeError("SENTINEL_LLM_PROFILES_JSON is not valid JSON.") from error

    if not isinstance(profiles, list):
        raise RuntimeError("SENTINEL_LLM_PROFILES_JSON must contain a JSON array.")

    for profile in profiles:
        if not isinstance(profile, dict) or profile.get("type") != "openai_compatible":
            raise RuntimeError("Each LLM profile must use type 'openai_compatible'.")
        name = str(profile.get("name", "")).strip()
        if not name or name == "mock" or name in providers:
            raise RuntimeError(f"Invalid or duplicate LLM profile name: '{name}'.")
        providers[name] = OpenAiCompatibleProvider(
            name=name,
            base_url=str(profile.get("base_url", "")).strip(),
            model=str(profile.get("model", "")).strip(),
            api_key_env=str(profile.get("api_key_env", "")).strip(),
            timeout_sec=float(profile.get("timeout_sec", 30.0)),
        )
    return providers
