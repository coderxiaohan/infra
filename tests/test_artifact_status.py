import unittest

from scripts.artifact_json import artifact_status_exit_code, is_non_success_artifact_status


class ArtifactStatusTest(unittest.TestCase):
    def test_benchmark_exit_code_requires_explicit_ok(self) -> None:
        self.assertEqual(artifact_status_exit_code({"status": "ok"}), 0)
        for status in ("failed", "skipped", "partial", "partial_no_timing", "planned"):
            with self.subTest(status=status):
                self.assertEqual(artifact_status_exit_code({"status": status}), 1)

    def test_allow_non_ok_is_explicit_escape_hatch(self) -> None:
        self.assertEqual(
            artifact_status_exit_code({"status": "skipped"}, allow_non_ok=True),
            0,
        )

    def test_generic_status_classifier_preserves_custom_success_states(self) -> None:
        self.assertTrue(is_non_success_artifact_status("probe_output_prepare_failed"))
        self.assertTrue(is_non_success_artifact_status("partial_no_metrics"))
        self.assertFalse(is_non_success_artifact_status("execute_verified"))
        self.assertFalse(is_non_success_artifact_status("catalog_registry_ready"))


if __name__ == "__main__":
    unittest.main()
