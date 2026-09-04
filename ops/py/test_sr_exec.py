import os
import sys
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, os.path.dirname(__file__))

try:
    import requests  # noqa: F401
except ModuleNotFoundError:
    sys.modules["requests"] = Mock()

from sr_exec import StarRocksClient


VALID_ENV = {
    "CLOUDBEAVER_BASE_URL": "https://flowops.example.internal/api/gql",
    "CLOUDBEAVER_USERNAME": "analyst",
    "CLOUDBEAVER_PASSWORD_HASH": "redacted-test-hash",
    "CLOUDBEAVER_PROJECT_ID": "analytics",
}


class StarRocksClientConfigTest(unittest.TestCase):
    @patch.dict(os.environ, {}, clear=True)
    def test_missing_configuration_fails_before_network_access(self):
        with self.assertRaisesRegex(RuntimeError, "CLOUDBEAVER_BASE_URL"):
            StarRocksClient()

    @patch.dict(os.environ, VALID_ENV, clear=True)
    def test_reads_connection_configuration_from_environment(self):
        client = StarRocksClient()
        self.assertEqual(client.base_url, VALID_ENV["CLOUDBEAVER_BASE_URL"])
        self.assertEqual(client.username, "analyst")
        self.assertEqual(client.project_id, "analytics")

    @patch.dict(
        os.environ,
        {
            **VALID_ENV,
            "CLOUDBEAVER_BASE_URL": "http://flowops.example.internal/api/gql",
        },
        clear=True,
    )
    def test_http_requires_explicit_opt_in(self):
        with self.assertRaisesRegex(RuntimeError, "CLOUDBEAVER_ALLOW_HTTP=1"):
            StarRocksClient()

    @patch.dict(
        os.environ,
        {
            **VALID_ENV,
            "CLOUDBEAVER_BASE_URL": "http://flowops.example.internal/api/gql",
            "CLOUDBEAVER_ALLOW_HTTP": "1",
        },
        clear=True,
    )
    def test_http_opt_in_allows_http(self):
        self.assertEqual(
            StarRocksClient().base_url,
            "http://flowops.example.internal/api/gql",
        )

    @patch.dict(
        os.environ,
        {
            **VALID_ENV,
            "CLOUDBEAVER_BASE_URL": "ftp://flowops.example.internal/api/gql",
            "CLOUDBEAVER_ALLOW_HTTP": "1",
        },
        clear=True,
    )
    def test_http_opt_in_does_not_allow_other_schemes(self):
        with self.assertRaisesRegex(RuntimeError, "CLOUDBEAVER_BASE_URL"):
            StarRocksClient()

    @patch.dict(os.environ, VALID_ENV, clear=True)
    def test_login_failure_does_not_echo_authentication_response(self):
        client = StarRocksClient()
        client.gql = Mock(
            side_effect=[
                {},
                {"data": {"authInfo": {"authStatus": "FAILED"}}, "secret": "hidden"},
            ]
        )
        with self.assertRaisesRegex(RuntimeError, "CloudBeaver login failed") as error:
            client.login()
        self.assertNotIn("hidden", str(error.exception))

    @patch.dict(os.environ, VALID_ENV, clear=True)
    def test_login_transport_failure_does_not_echo_response_content(self):
        client = StarRocksClient()
        client.gql = Mock(side_effect=Exception("hidden response content"))
        with self.assertRaisesRegex(RuntimeError, "^CloudBeaver login failed$") as error:
            client.login()
        self.assertNotIn("hidden response content", str(error.exception))
