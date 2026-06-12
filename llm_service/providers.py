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

    def available(self) -> bool:
        ...

    def reason(self, scene_state: dict[str, Any]) -> ReasoningResult:
        ...


class MockProvider:
    model = "sentinel-rules-v1"

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


def create_providers() -> dict[str, ReasoningProvider]:
    return {"mock": MockProvider()}
