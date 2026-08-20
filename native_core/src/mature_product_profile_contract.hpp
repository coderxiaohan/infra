#pragma once

#include <string>
#include <vector>

namespace inferlab::mature_product_profiles {

enum class ProductProfileFieldKind {
  string,
  object,
  array,
  boolean,
};

struct ProductProfileFieldSpec {
  std::string name;
  ProductProfileFieldKind kind = ProductProfileFieldKind::string;
};

const std::vector<ProductProfileFieldSpec>&
product_profile_required_artifact_field_specs();

const std::vector<std::string>& product_profile_required_artifact_fields();

}  // namespace inferlab::mature_product_profiles
