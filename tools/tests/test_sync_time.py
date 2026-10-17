"""PC helper transport tests; no device or network required."""

import unittest
from unittest.mock import Mock, patch

from test_gen_tools import load_tool


class SyncTimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sync = load_tool("sync_time")

    def test_verified_handshake_then_utc_and_close(self):
        port = Mock()
        with patch.object(self.sync.serial, "Serial", return_value=port), \
             patch.object(self.sync.time, "time", return_value=1792249200.9), \
             patch.object(self.sync, "wait_line", side_effect=[b"FLINT HELLO 1 flint_spark",
                                                            b"FLINT TIME OK 1792249200"]):
            self.assertEqual(self.sync.synchronize("test"), 1792249200)
        self.assertEqual([c.args[0] for c in port.write.call_args_list],
                         [b"@HELLO\n", b"@TIME 1792249200\n"])
        self.assertFalse(port.dtr)
        self.assertFalse(port.rts)
        port.close.assert_called_once()

    def test_no_time_sent_to_unrecognized_firmware(self):
        port = Mock()
        with patch.object(self.sync.serial, "Serial", return_value=port), \
             patch.object(self.sync, "wait_line", side_effect=TimeoutError):
            with self.assertRaises(TimeoutError):
                self.sync.synchronize("test")
        self.assertEqual([c.args[0] for c in port.write.call_args_list], [b"@HELLO\n"] * 3)
        port.close.assert_called_once()

    def test_mismatched_acknowledgement_is_failure(self):
        port = Mock()
        with patch.object(self.sync.serial, "Serial", return_value=port), \
             patch.object(self.sync.time, "time", return_value=1792249200), \
             patch.object(self.sync, "wait_line", side_effect=[b"FLINT HELLO 1 flint_spark",
                                                            b"FLINT TIME OK 1792249199"]):
            with self.assertRaises(ValueError):
                self.sync.synchronize("test")
        port.close.assert_called_once()


if __name__ == "__main__":
    unittest.main()
