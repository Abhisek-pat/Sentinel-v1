import json
import os
from typing import Any

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from openai import OpenAI

app = FastAPI()

client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY"))


class SceneRequest(BaseModel):
    scene_state: dict[str, Any]


@app.post("/reason")
def reason_over_scene(req: SceneRequest) -> dict[str, Any]:
    if not os.environ.get("OPENAI_API_KEY"):
        raise HTTPException(status_code=500, detail="OPENAI_API_KEY is not set")

    scene_json = json.dumps(req.scene_state, indent=2)

    developer_prompt = (
        "You are a surveillance reasoning engine. "
        "Use only the provided structured scene state. "
        "Do not invent people, zones, or events. "
        "Return strict JSON with summary, risk_level, and recommended_action."
    )

    user_prompt = f"""
Given this scene state:

{scene_json}

Return JSON in this format:
{{
  "summary": "string",
  "risk_level": "low | medium | high",
  "recommended_action": "string"
}}
"""

    try:
        response = client.responses.create(
            model="gpt-4.1-mini",
            input=[
                {"role": "developer", "content": developer_prompt},
                {"role": "user", "content": user_prompt},
            ],
            text={
                "format": {
                    "type": "json_schema",
                    "name": "scene_reasoning",
                    "schema": {
                        "type": "object",
                        "additionalProperties": False,
                        "properties": {
                            "summary": {"type": "string"},
                            "risk_level": {
                                "type": "string",
                                "enum": ["low", "medium", "high"]
                            },
                            "recommended_action": {"type": "string"}
                        },
                        "required": ["summary", "risk_level", "recommended_action"]
                    }
                }
            }
        )

        raw_text = response.output_text
        return json.loads(raw_text)

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))