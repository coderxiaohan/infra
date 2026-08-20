#!/usr/bin/env python3
try:
    from scripts.product_profiles.common import main_for_product
except ModuleNotFoundError:
    from common import main_for_product

if __name__ == "__main__":
    raise SystemExit(main_for_product("sglang"))
