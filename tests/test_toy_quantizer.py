import unittest

from quantization.toy_quantizer import quantize_group


class ToyQuantizerTest(unittest.TestCase):
    def test_asymmetric_positive_group_has_bounded_zero_point(self) -> None:
        result = quantize_group([2.0, 3.0, 4.0], bits=4, symmetric=False)

        self.assertGreaterEqual(result["zero_point"], 0)
        self.assertLessEqual(result["zero_point"], 15)
        self.assertLessEqual(result["max_abs_error"], result["scale"])

    def test_asymmetric_negative_group_has_bounded_zero_point(self) -> None:
        result = quantize_group([-4.0, -3.0, -2.0], bits=4, symmetric=False)

        self.assertGreaterEqual(result["zero_point"], 0)
        self.assertLessEqual(result["zero_point"], 15)
        self.assertLessEqual(result["max_abs_error"], result["scale"])


if __name__ == "__main__":
    unittest.main()
