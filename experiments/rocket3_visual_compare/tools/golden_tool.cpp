#include "canonical_scenes.h"
#include "image_io.h"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 9 || std::string(argv[1]) != "--output" ||
      std::string(argv[3]) != "--approval-id" ||
      std::string(argv[5]) != "--approved-by" ||
      std::string(argv[7]) != "--approved-at") {
    std::cerr << "usage: rocket_phase32_golden_tool --output <directory> "
                 "--approval-id <id> --approved-by <reviewer> "
                 "--approved-at <date>\n";
    return 2;
  }
  const std::filesystem::path output = argv[2];
  std::error_code filesystemError;
  std::filesystem::create_directories(output, filesystemError);
  if (filesystemError) {
    std::cerr << "could not create golden output directory\n";
    return 1;
  }
  for (const auto& scene :
       rocket3::visual_compare::render_canonical_scenes()) {
    rocket3::visual_compare::GoldenApproval approval;
    approval.status =
        rocket3::visual_compare::GoldenApproval::Status::Approved;
    approval.approval_id = argv[4];
    approval.scene_key = "scene:" + scene.name;
    approval.baseline_hash = "reviewed-source-tree";
    approval.approved_by = argv[6];
    approval.approved_at = argv[8];
    approval.update_allowed = true;
    std::string error;
    const auto path = output / (scene.name + ".ppm");
    if (!rocket3::visual_compare::update_approved_golden(
            path, scene.image, approval, error)) {
      std::cerr << scene.name << ": " << error << '\n';
      return 1;
    }
    if (!rocket3::visual_compare::write_png(
            output / (scene.name + ".png"), scene.image, error)) {
      std::cerr << scene.name << " preview: " << error << '\n';
      return 1;
    }
    std::cout << path.generic_string() << '\n';
  }
  return 0;
}
