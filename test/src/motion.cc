import std;
import skia;
import skiff.scene;
import skiff.compose;
import skiff.model;
import skiff.bind;
import skiff.widgets.motion;
import gtest;

#include "gtest/gtest-macros.h"

namespace {
namespace c = skiff::compose;
namespace scene = skiff::scene;
struct folded_state { bool open = true; };
struct fold_root { skiff::model::Tracked<folded_state> state; };
using fold_model = skiff::model::Model<fold_root, skiff::bind::NoReactions>;
const auto area = skia::SkRect::MakeXYWH(0.0f, 0.0f, 200.0f, 200.0f);
}

TEST(MotionBinding, FirstValueIsImmediateAndLaterValuesFoldTheSameChild) {
  fold_model model(fold_root{});
  skiff::bind::Binding<fold_model> binding;
  const auto content = [] { return c::column(c::vbox(0.0f, {.fillX = true, .height = 40.0f})); };
  auto expression = c::projected<folded_state>([](const folded_state& state) { return state.open; },
      skiff::widgets::collapsible(content()));
  scene::Scene tree(std::move(expression));
  // The Collapsible, under what the view was built as: its child the node
  // the column builds.
  using Folding = skiff::widgets::Collapsible<decltype(c::build(content()))>;
  Folding* const folding = scene::findNode<Folding>(tree.root());
  ASSERT_NE(folding, nullptr);
  binding.refresh(tree.root(), model);
  EXPECT_TRUE(folding->isOpen());
  EXPECT_TRUE(folding->child().visible());
  const auto child = folding->child().id();
  tree.layoutIfNeeded(area);
  tree.update(1000.0);
  tree.layoutIfNeeded(area);
  EXPECT_FLOAT_EQ(folding->bounds().height(), 40.0f);
  EXPECT_FALSE(folding->settling());
  folding->read(true);
  EXPECT_TRUE(folding->fState.fLayoutValid);
  model.apply(skiff::model::over<folded_state>(skiff::model::setTo(folded_state{false})));
  binding.refresh(tree.root(), model);
  EXPECT_FALSE(folding->isOpen());
  EXPECT_TRUE(folding->child().visible());
  EXPECT_TRUE(folding->settling());
  for (int step = 1; step <= 50; ++step) {
    tree.update(1000.0 + step * 20.0);
    tree.layoutIfNeeded(area);
  }
  EXPECT_FLOAT_EQ(folding->bounds().height(), 0.0f);
  EXPECT_FALSE(folding->child().visible());
  EXPECT_EQ(folding->child().id(), child);
  model.apply(skiff::model::over<folded_state>(skiff::model::setTo(folded_state{true})));
  binding.refresh(tree.root(), model);
  for (int step = 1; step <= 50; ++step) {
    tree.update(2000.0 + step * 20.0);
    tree.layoutIfNeeded(area);
  }
  EXPECT_FLOAT_EQ(folding->bounds().height(), 40.0f);
  EXPECT_EQ(folding->child().id(), child);
}
