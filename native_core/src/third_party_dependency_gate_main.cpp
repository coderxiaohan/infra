#include "dependency_governance_cli.hpp"

int main(int argc, char** argv) {
  return inferlab::dependency_governance::run_cli(
      argc, argv,
      inferlab::dependency_governance::ArtifactView::policy_gate);
}
