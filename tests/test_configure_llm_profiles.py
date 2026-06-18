import importlib.util
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path


SCRIPT_PATH = Path(__file__).parents[1] / "deploy" / "pi5" / "configure_llm_profiles.py"
SPEC = importlib.util.spec_from_file_location("configure_llm_profiles", SCRIPT_PATH)
configure_llm_profiles = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(configure_llm_profiles)


class ConfigureLlmProfilesTest(unittest.TestCase):
    def test_renders_valid_env(self) -> None:
        profile = configure_llm_profiles.parse_profile(
            "name=openai-fast,base_url=https://api.openai.com/v1,"
            "model=gpt-4.1-mini,api_key_env=OPENAI_API_KEY,timeout_sec=45"
        )
        rendered = configure_llm_profiles.render_env(
            [profile],
            default_provider="openai-fast",
            fallback_provider="mock",
        )
        self.assertIn("SENTINEL_LLM_PROVIDER=openai-fast", rendered)
        self.assertIn("\"api_key_env\":\"OPENAI_API_KEY\"", rendered)

    def test_rejects_unknown_default_provider(self) -> None:
        profile = configure_llm_profiles.parse_profile(
            "name=model-a,base_url=http://127.0.0.1:11434/v1,model=a"
        )
        with self.assertRaises(ValueError):
            configure_llm_profiles.render_env([profile], "missing", "mock")

    def test_cli_writes_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "llm.env"
            argv = sys.argv
            try:
                sys.argv = [
                    "configure_llm_profiles.py",
                    "--profile",
                    "name=model-a,base_url=http://127.0.0.1:11434/v1,model=a",
                    "--default-provider",
                    "model-a",
                    "--output",
                    str(output),
                ]
                with redirect_stdout(StringIO()):
                    self.assertEqual(0, configure_llm_profiles.main())
            finally:
                sys.argv = argv
            self.assertIn("SENTINEL_LLM_PROFILES_JSON", output.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
