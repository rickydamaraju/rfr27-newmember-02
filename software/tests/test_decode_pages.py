import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from decode_pages import crc8, decode


def page(number, sequence=0x1234):
    data = bytearray(24)
    data[0] = 1
    data[1] = number
    data[2:4] = sequence.to_bytes(2, "little")
    data[4] = 4  # Firmware FLAG_UNCALIBRATED
    if number == 0:
        data[5] = 15
        data[6:8] = (0xFFFF).to_bytes(2, "little")
        data[10:12] = (6500).to_bytes(2, "little", signed=True)
        data[12:14] = (2500).to_bytes(2, "little", signed=True)
        data[14:16] = (3000).to_bytes(2, "little", signed=True)
        data[16:20] = (123456).to_bytes(4, "little")
    else:
        data[5] = (number - 1) * 8
        for i in range(8):
            t = 2500 + ((number - 1) * 8 + i) * 100
            data[6 + i * 2:8 + i * 2] = t.to_bytes(2, "little", signed=True)
    data[23] = crc8(data[:23])
    return data


class DecoderTest(unittest.TestCase):
    def test_complete_scan(self):
        result = decode([page(0), page(1), page(2)])
        self.assertEqual(result["sequence"], 0x1234)
        self.assertEqual(result["maximum_c"], 65)
        self.assertEqual(result["max_channel"], 15)
        self.assertEqual(result["temperature_c"][15], 40)
        self.assertEqual(result["uptime_ms"], 123456)
        self.assertFalse(result["all_readings_validated"])

    def test_corruption_and_mixed_scans_rejected(self):
        packets = [page(0), page(1), page(2)]
        packets[1][10] ^= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            decode(packets)
        with self.assertRaisesRegex(ValueError, "Mixed scans"):
            decode([page(0), page(1, 0x1235), page(2)])


if __name__ == "__main__":
    unittest.main()
