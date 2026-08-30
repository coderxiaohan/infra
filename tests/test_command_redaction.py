import unittest

from scripts.command_redaction import REDACTED_VALUE, redact_sensitive_text, redact_value


class CommandRedactionTest(unittest.TestCase):
    def test_free_text_redacts_sensitive_suffixes_and_private_keys(self) -> None:
        text = (
            "hf_token=hf-secret private_key=key-material "
            "client_secret_key=client-material"
        )

        redacted = redact_sensitive_text(text)

        self.assertNotIn("hf-secret", redacted)
        self.assertNotIn("key-material", redacted)
        self.assertNotIn("client-material", redacted)
        self.assertEqual(redacted.count(REDACTED_VALUE), 3)

    def test_free_text_redacts_truncated_single_line_pem_value(self) -> None:
        redacted = redact_sensitive_text(
            "private_key=-----BEGIN PRIVATE KEY-----abc"
        )

        self.assertEqual(redacted, f"private_key={REDACTED_VALUE}")
        self.assertNotIn("PRIVATE KEY", redacted)

    def test_free_text_redacts_complete_multiline_pem_record(self) -> None:
        text = (
            "private_key=-----BEGIN PRIVATE KEY-----\n"
            "YWJjZA==\n"
            "-----END PRIVATE KEY-----\n"
            "status=ok"
        )

        redacted = redact_sensitive_text(text)

        self.assertEqual(redacted, f"private_key={REDACTED_VALUE}\nstatus=ok")
        self.assertNotIn("YWJjZA", redacted)

    def test_truncated_multiline_pem_stops_at_next_field(self) -> None:
        text = (
            "private_key=-----BEGIN PRIVATE KEY-----\n"
            "secret body\n"
            "status=ok\n"
            "message=preserved"
        )

        redacted = redact_sensitive_text(text)

        self.assertEqual(
            redacted,
            f"private_key={REDACTED_VALUE}\nstatus=ok\nmessage=preserved",
        )

    def test_quoted_multiline_secret_is_fully_redacted(self) -> None:
        text = 'client_secret="first words\nsecond words" status=ok'

        redacted = redact_sensitive_text(text)

        self.assertEqual(
            redacted,
            f'client_secret="{REDACTED_VALUE}" status=ok',
        )

    def test_non_sensitive_pem_record_is_preserved(self) -> None:
        text = (
            "certificate=-----BEGIN CERTIFICATE-----\n"
            "public-data\n"
            "-----END CERTIFICATE-----"
        )

        self.assertEqual(redact_sensitive_text(text), text)

    def test_free_text_redacts_bare_user_password(self) -> None:
        redacted = redact_sensitive_text("login failed for user:pass")

        self.assertNotIn("user:pass", redacted)
        self.assertIn(REDACTED_VALUE, redacted)

    def test_non_secret_key_value_text_is_preserved(self) -> None:
        text = "status=ok cache_key=abc label:value"
        self.assertEqual(redact_sensitive_text(text), text)

    def test_structured_config_redacts_new_sensitive_names(self) -> None:
        redacted = redact_value(
            {
                "private_key": "one",
                "client_secret_key": "two",
                "hf_token": "three",
                "cache_key": "keep",
            }
        )

        self.assertEqual(redacted["private_key"], REDACTED_VALUE)
        self.assertEqual(redacted["client_secret_key"], REDACTED_VALUE)
        self.assertEqual(redacted["hf_token"], REDACTED_VALUE)
        self.assertEqual(redacted["cache_key"], "keep")


if __name__ == "__main__":
    unittest.main()
