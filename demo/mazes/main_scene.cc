// The same settings as a scene on Skia: the tree laid out in a viewport of
// the Qt window's size, and said to be. A window to show it in is a host's
// business (mux's, osu-cpp's); this proves the one source builds for the
// scene as it builds for the page.
import std;
import skia;
import skiff.ui;
#include "page.h"

int main() {
  Model model;
  scene::Scene tree(page());
  bind::Binding<Model> binding;
  binding.refresh(tree.root(), model);
  const bool laid = tree.layoutIfNeeded(skia::SkRect::MakeWH(448.0f, 960.0f));
  std::println("mazes settings as a scene: laid out {}", laid ? "yes" : "no");
  return 0;
}
