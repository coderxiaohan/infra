import tempfile
import unittest
from pathlib import Path

from scripts.generate_gguf_fixture import build_synthetic_gguf_fixture, write_fixture


def read_u32(payload: bytes, offset: int) -> int:
    return int.from_bytes(payload[offset : offset + 4], byteorder="little")


def read_u64(payload: bytes, offset: int) -> int:
    return int.from_bytes(payload[offset : offset + 8], byteorder="little")


class GGUFFixtureGeneratorTest(unittest.TestCase):
    def test_fixture_matches_probe_layout_contract(self):
        payload = build_synthetic_gguf_fixture()

        self.assertEqual(payload[:4], b"GGUF")
        self.assertEqual(read_u32(payload, 4), 3)
        self.assertEqual(read_u64(payload, 8), 28)
        self.assertEqual(read_u64(payload, 16), 7)
        self.assertEqual(len(payload), 2016 + 3520)

        tensor_data_start = 2016
        q4_k_start = tensor_data_start + 512
        q5_k_start = tensor_data_start + 1088
        q6_k_start = tensor_data_start + 1440
        q3_k_start = tensor_data_start + 1664
        q2_k_start = tensor_data_start + 1792
        q8_0_start = tensor_data_start + 1920
        q4_0_start = tensor_data_start + 2208
        q4_1_start = tensor_data_start + 2240
        q5_0_start = tensor_data_start + 2272
        q5_1_start = tensor_data_start + 2304
        q8_1_start = tensor_data_start + 2336
        iq4_nl_start = tensor_data_start + 2400
        q8_k_start = tensor_data_start + 2432
        q1_0_start = tensor_data_start + 2752
        iq4_xs_start = tensor_data_start + 2784
        tq2_0_start = tensor_data_start + 2944
        tq1_0_start = tensor_data_start + 3040
        mxfp4_start = tensor_data_start + 3104
        nvfp4_start = tensor_data_start + 3136
        bf16_start = tensor_data_start + 3200
        f16_start = tensor_data_start + 3232
        f64_start = tensor_data_start + 3264
        i32_start = tensor_data_start + 3296
        i16_start = tensor_data_start + 3328
        i8_start = tensor_data_start + 3360
        i64_start = tensor_data_start + 3392
        iq2_xxs_start = tensor_data_start + 3424
        self.assertEqual(
            list(payload[tensor_data_start : tensor_data_start + 32]),
            [
                0x00,
                0x00,
                0x80,
                0x3F,
                0x00,
                0x00,
                0x80,
                0xBF,
                0x00,
                0x00,
                0x00,
                0x3F,
                0x00,
                0x00,
                0x00,
                0xBF,
                0x00,
                0x00,
                0x00,
                0x40,
                0x00,
                0x00,
                0x00,
                0xC0,
                0x00,
                0x00,
                0x60,
                0x40,
                0x00,
                0x00,
                0x80,
                0xC0,
            ],
        )
        self.assertEqual(payload[q4_k_start : q4_k_start + 4], b"\x00\x3c\x00\x38")
        self.assertEqual(
            list(payload[q4_k_start + 16 : q4_k_start + 24]),
            list(range(8)),
        )
        self.assertEqual(payload[q5_k_start : q5_k_start + 4], b"\x00\x3c\x00\x38")
        self.assertEqual(list(payload[q5_k_start + 16 : q5_k_start + 20]), [255] * 4)
        self.assertEqual(
            list(payload[q5_k_start + 48 : q5_k_start + 56]),
            list(range(8)),
        )
        self.assertEqual(list(payload[q6_k_start : q6_k_start + 8]), list(range(8)))
        self.assertEqual(list(payload[q6_k_start + 128 : q6_k_start + 132]), [255] * 4)
        self.assertEqual(list(payload[q6_k_start + 192 : q6_k_start + 196]), [0, 1, 2, 3])
        self.assertEqual(payload[q6_k_start + 208 : q6_k_start + 210], b"\x00\x3c")
        self.assertEqual(payload[q3_k_start : q3_k_start + 4], b"\x00\xff\xff\xff")
        self.assertEqual(
            list(payload[q3_k_start + 32 : q3_k_start + 40]),
            list(range(8)),
        )
        self.assertEqual(
            list(payload[q3_k_start + 96 : q3_k_start + 100]),
            [0x80, 0x91, 0xA2, 0xB3],
        )
        self.assertEqual(payload[q3_k_start + 108 : q3_k_start + 110], b"\x00\x3c")
        self.assertEqual(
            list(payload[q2_k_start : q2_k_start + 4]),
            [0x00, 0x11, 0x22, 0x33],
        )
        self.assertEqual(
            list(payload[q2_k_start + 16 : q2_k_start + 24]),
            list(range(8)),
        )
        self.assertEqual(payload[q2_k_start + 80 : q2_k_start + 84], b"\x00\x3c\x00\x38")
        self.assertEqual(payload[q8_0_start : q8_0_start + 2], b"\x00\x3c")
        self.assertEqual(
            list(payload[q8_0_start + 2 : q8_0_start + 6]),
            [240, 241, 242, 243],
        )
        self.assertEqual(payload[q4_0_start : q4_0_start + 2], b"\x00\x3c")
        self.assertEqual(
            list(payload[q4_0_start + 2 : q4_0_start + 6]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(payload[q4_1_start : q4_1_start + 4], b"\x00\x3c\x00\x38")
        self.assertEqual(
            list(payload[q4_1_start + 4 : q4_1_start + 8]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(payload[q5_0_start : q5_0_start + 2], b"\x00\x3c")
        self.assertEqual(
            list(payload[q5_0_start + 2 : q5_0_start + 6]),
            [0x00, 0xFF, 0x55, 0xAA],
        )
        self.assertEqual(
            list(payload[q5_0_start + 6 : q5_0_start + 10]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(payload[q5_1_start : q5_1_start + 4], b"\x00\x3c\x00\x38")
        self.assertEqual(
            list(payload[q5_1_start + 4 : q5_1_start + 8]),
            [0x00, 0xFF, 0x55, 0xAA],
        )
        self.assertEqual(
            list(payload[q5_1_start + 8 : q5_1_start + 12]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(payload[q8_1_start : q8_1_start + 4], b"\x00\x00\x00\x3f")
        self.assertEqual(payload[q8_1_start + 4 : q8_1_start + 8], b"\x00\x00\x00\xc1")
        self.assertEqual(
            list(payload[q8_1_start + 8 : q8_1_start + 12]),
            [240, 241, 242, 243],
        )
        self.assertEqual(payload[iq4_nl_start : iq4_nl_start + 2], b"\x00\x38")
        self.assertEqual(
            list(payload[iq4_nl_start + 2 : iq4_nl_start + 6]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(payload[q8_k_start : q8_k_start + 4], b"\x00\x00\x80\x3e")
        self.assertEqual(
            list(payload[q8_k_start + 4 : q8_k_start + 8]),
            [128, 129, 130, 131],
        )
        self.assertEqual(
            payload[q8_k_start + 260 : q8_k_start + 264],
            b"\x78\xf8\x78\xf9",
        )
        self.assertEqual(payload[q8_k_start + 276 : q8_k_start + 278], b"\x78\x00")
        self.assertEqual(payload[q8_k_start + 290 : q8_k_start + 292], b"\x78\x07")
        self.assertEqual(payload[q1_0_start : q1_0_start + 2], b"\x00\x3c")
        self.assertEqual(list(payload[q1_0_start + 2 : q1_0_start + 6]), [0xAA] * 4)
        self.assertEqual(payload[iq4_xs_start : iq4_xs_start + 4], b"\x00\x3c\xaa\xaa")
        self.assertEqual(
            list(payload[iq4_xs_start + 4 : iq4_xs_start + 8]),
            [0x21, 0x43, 0x65, 0x87],
        )
        self.assertEqual(
            list(payload[iq4_xs_start + 8 : iq4_xs_start + 12]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(list(payload[tq2_0_start : tq2_0_start + 4]), [0x24] * 4)
        self.assertEqual(payload[tq2_0_start + 64 : tq2_0_start + 66], b"\x00\x3c")
        self.assertEqual(list(payload[tq1_0_start : tq1_0_start + 4]), [0x00] * 4)
        self.assertEqual(
            list(payload[tq1_0_start + 16 : tq1_0_start + 20]),
            [0x80] * 4,
        )
        self.assertEqual(
            list(payload[tq1_0_start + 32 : tq1_0_start + 36]),
            [0xFF] * 4,
        )
        self.assertEqual(payload[tq1_0_start + 48 : tq1_0_start + 54], b"\x00\x80\xff\x80\x00\x3c")
        self.assertEqual(payload[mxfp4_start], 0x80)
        self.assertEqual(
            list(payload[mxfp4_start + 1 : mxfp4_start + 5]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(list(payload[nvfp4_start : nvfp4_start + 4]), [0x38, 0x40, 0x30, 0x48])
        self.assertEqual(
            list(payload[nvfp4_start + 4 : nvfp4_start + 8]),
            [0xF0, 0xE1, 0xD2, 0xC3],
        )
        self.assertEqual(
            list(payload[bf16_start : bf16_start + 16]),
            [
                0x80,
                0x3F,
                0x00,
                0x40,
                0x20,
                0xC0,
                0x00,
                0x00,
                0x00,
                0x3F,
                0x00,
                0xBF,
                0xC0,
                0x3F,
                0x00,
                0xC0,
            ],
        )
        self.assertEqual(
            list(payload[f16_start : f16_start + 16]),
            [
                0x00,
                0x3C,
                0x00,
                0x40,
                0x00,
                0xC1,
                0x00,
                0x00,
                0x00,
                0x38,
                0x00,
                0xB8,
                0x00,
                0x43,
                0x00,
                0xC4,
            ],
        )
        self.assertEqual(
            list(payload[f64_start : f64_start + 32]),
            [
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0xF0,
                0x3F,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0xF0,
                0xBF,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0xD0,
                0x3F,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0xD0,
                0xBF,
            ],
        )
        self.assertEqual(
            list(payload[i32_start : i32_start + 16]),
            [
                0x01,
                0x00,
                0x00,
                0x00,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
                0x40,
                0xE2,
                0x01,
                0x00,
                0xC0,
                0x1D,
                0xFE,
                0xFF,
            ],
        )
        self.assertEqual(
            list(payload[i16_start : i16_start + 8]),
            [
                0x01,
                0x00,
                0xFF,
                0xFF,
                0xD2,
                0x04,
                0x2E,
                0xFB,
            ],
        )
        self.assertEqual(
            list(payload[i8_start : i8_start + 4]),
            [0x01, 0xFF, 0x64, 0x9C],
        )
        self.assertEqual(
            list(payload[i64_start : i64_start + 32]),
            [
                0x01,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
                0x15,
                0xCD,
                0x5B,
                0x07,
                0x00,
                0x00,
                0x00,
                0x00,
                0xEB,
                0x32,
                0xA4,
                0xF8,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
            ],
        )
        self.assertEqual(
            list(payload[iq2_xxs_start : iq2_xxs_start + 18]),
            [
                0x00,
                0x3C,
                0x01,
                0x02,
                0x03,
                0x04,
                0x01,
                0xC1,
                0x80,
                0x10,
                0x05,
                0x06,
                0x07,
                0x08,
                0x05,
                0xC3,
                0x01,
                0x21,
            ],
        )

    def test_write_fixture_creates_parent_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "nested" / "inferlab_fixture.gguf"

            size = write_fixture(output)

            self.assertEqual(size, 2016 + 3520)
            self.assertEqual(output.read_bytes()[:4], b"GGUF")


if __name__ == "__main__":
    unittest.main()
