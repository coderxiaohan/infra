#include "scheduler_deep_dive_cli.hpp"

int main(int argc, char** argv) {
  return inferlab::scheduler_deep_dive::run_cli(
      inferlab::scheduler_deep_dive::ArtifactRole::root_cause, argc, argv);
}
