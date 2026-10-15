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
  auto expression = c::projected<folded_state>([](const folded_state& state) { return state.open; },
      skiff::widgets::Collapsible(c::column(c::vbox(0.0f, {.fillX = true, .height = 40.0f}))));
  scene::Scene tree(std::move(expression));
  binding.refresh(tree.root(), model);
  EXPECT_TRUE(tree.root().isOpen());
  EXPECT_TRUE(tree.root().child().visible());
  const auto child = tree.root().child().id();
  tree.layoutIfNeeded(area);
  tree.update(1000.0);
  tree.layoutIfNeeded(area);
  EXPECT_FLOAT_EQ(tree.root().bounds().height(), 40.0f);
  EXPECT_FALSE(tree.root().settling());
  tree.root().read(folded_state{true});
  EXPECT_TRUE(tree.root().fState.fLayoutValid);
  model.apply(skiff::model::over<folded_state>(skiff::model::setTo(folded_state{false})));
  binding.refresh(tree.root(), model);
  EXPECT_FALSE(tree.root().isOpen());
  EXPECT_TRUE(tree.root().child().visible());
  EXPECT_TRUE(tree.root().settling());
  for (int step = 1; step <= 50; ++step) {
    tree.update(1000.0 + step * 20.0);
    tree.layoutIfNeeded(area);
  }
  EXPECT_FLOAT_EQ(tree.root().bounds().height(), 0.0f);
  EXPECT_FALSE(tree.root().child().visible());
  EXPECT_EQ(tree.root().child().id(), child);
  model.apply(skiff::model::over<folded_state>(skiff::model::setTo(folded_state{true})));
  binding.refresh(tree.root(), model);
  for (int step = 1; step <= 50; ++step) {
    tree.update(2000.0 + step * 20.0);
    tree.layoutIfNeeded(area);
  }
  EXPECT_FLOAT_EQ(tree.root().bounds().height(), 40.0f);
  EXPECT_EQ(tree.root().child().id(), child);
}
