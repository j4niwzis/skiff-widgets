import std;
import gtest;
import skia;
import skiff.scene;
import skiff.nodes.box;
import skiff.widgets.motion;
import skiff.widgets.button;
import skiff.widgets.dropdown;
import skiff.widgets.sliderbar;
import skiff.widgets.textbox;
import skiff.widgets.model;

#include "gtest/gtest-macros.h"

namespace {

namespace scene = skiff::scene;
namespace widgets = skiff::widgets;

// A screen of one node, for the tests of that node.
template <class Child> struct One : scene::Node {
  Child child;
  explicit One(Child made) : child(std::move(made)) {}
  void forEachChild(auto &&f) { f(child); }
};
template <class Child> One(Child) -> One<Child>;

template <class Child>
auto sceneOf(const scene::Spec &spec, Child child) {
  return std::make_unique<scene::Scene<One<Child>>>(
      std::in_place, scene::placed(spec, std::move(child)));
}

TEST(RangeSlider, OwnsHandleSelectionDragAndMinimumSpan) {
  int changes = 0;
  auto made = sceneOf({.x = 10.0f,
                       .y = 13.0f,
                       .width = 100.0f,
                       .height = 14.0f,
                       .relativeSize = scene::axes::kNone},
                      widgets::RangeSlider([&changes](float, float) { ++changes; }));
  auto &s = *made;
  auto &slider = s.root().child;
  slider.setMinimumSpan(0.1f);
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(200.0f, 40.0f));

  // Half way is equally close to both ends, so the lower handle wins.
  EXPECT_TRUE(s.click(60.0f, 20.0f));
  EXPECT_TRUE(slider.dragging());
  EXPECT_FLOAT_EQ(slider.low(), 0.5f);
  EXPECT_FLOAT_EQ(slider.high(), 1.0f);

  slider.dragTo(200.0f);
  EXPECT_NEAR(slider.low(), 0.9f, 0.0001f);
  EXPECT_FLOAT_EQ(slider.high(), 1.0f);
  EXPECT_EQ(changes, 2);

  slider.endDrag();
  EXPECT_FALSE(slider.dragging());
}

struct Opened {
  int *opened;
  void operator()() const { ++*opened; }
};
struct Chosen {
  int *chosen;
  scene::Node **list;
  void operator()(int index) const {
    *chosen = index;
    (*list)->setVisible(false);
  }
};
struct DropdownScreen : scene::Node {
  widgets::DropdownButton<Opened> button;
  widgets::DropdownList<Chosen> list;
  DropdownScreen(int *opened, int *chosen, scene::Node **listNode)
      : button(scene::make<widgets::DropdownButton<Opened>>(
            {.width = 120.0f, .height = 30.0f}, "Sort", "Title",
            Opened{opened})),
        list(scene::make<widgets::DropdownList<Chosen>>(
            {.y = 34.0f, .width = 120.0f}, Chosen{chosen, listNode})) {}
  void forEachChild(auto &&f) {
    f(button);
    f(list);
  }
};

TEST(Dropdown, ButtonAndRowsRouteTheirOwnClicks) {
  int opened = 0;
  int chosen = -1;
  scene::Node *listNode = nullptr;
  scene::Scene<DropdownScreen> s{std::in_place, &opened, &chosen, &listNode};
  s.state().apply({.fill = true});
  auto &list = s.root().list;
  listNode = &list;
  list.setOptions({"Artist", "Title"});
  list.setCurrent(1);
  s.layoutIfNeeded(skia::SkRect::MakeWH(200.0f, 120.0f));

  EXPECT_TRUE(list.expanded());
  EXPECT_TRUE(s.click(20.0f, 15.0f));
  EXPECT_EQ(opened, 1);
  scene::PointerEvent down = scene::pointer::down{20.0f, 64.0f};
  EXPECT_TRUE(s.dispatchPointer(down));
  EXPECT_EQ(chosen, 1);
  EXPECT_FALSE(list.expanded());
  EXPECT_NE(s.capturedId(), 0u);
  EXPECT_TRUE(s.dispatchPointer(scene::pointer::up{20.0f, 64.0f}));
  EXPECT_EQ(chosen, 1);
  EXPECT_EQ(s.capturedId(), 0u);

  EXPECT_FALSE(list.expanded());
  EXPECT_FALSE(s.click(20.0f, 64.0f));
}

TEST(Button, PrimaryAndEnabledStateOwnDamageAndInput) {
  int clicks = 0;
  // Named by its action's type: a widget's public name is an alias, which a
  // deduction cannot see through.
  const auto render = [&clicks] { ++clicks; };
  auto made = sceneOf({.width = 100.0f, .height = 30.0f},
                      widgets::Button<decltype(render)>("Render", render));
  auto &s = *made;
  auto &button = s.root().child;
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(120.0f, 50.0f));
  (void)s.finishFrame();

  button.setPrimary(true);
  EXPECT_TRUE(button.primary());
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());
  button.setOutlined(true);
  EXPECT_TRUE(button.outlined());
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());
  button.setAccent(0xff123456);
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());
  EXPECT_TRUE(s.click(20.0f, 15.0f));
  EXPECT_EQ(clicks, 1);

  button.setEnabled(false);
  EXPECT_FALSE(button.enabled());
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());
  EXPECT_FALSE(s.click(20.0f, 15.0f));
  EXPECT_EQ(clicks, 1);

  s.setHover(-1.0f, -1.0f);
  (void)s.finishFrame();
  s.setHover(20.0f, 15.0f);
  EXPECT_TRUE(s.finishFrame().fDamage.isEmpty());
}

struct Nothing {
  void operator()(float) const {}
};
struct Nowhere {
  void operator()() const {}
};
struct HoverScreen : scene::Node {
  widgets::SliderBar<Nothing> slider = scene::make<widgets::SliderBar<Nothing>>(
      {.width = 100.0f, .height = 14.0f}, Nothing{});
  widgets::Button<Nowhere> button = scene::make<widgets::Button<Nowhere>>(
      {.y = 20.0f, .width = 100.0f, .height = 30.0f}, "Apply", Nowhere{});
  void forEachChild(auto &&f) {
    f(slider);
    f(button);
  }
};

TEST(Hover, OnlyVisibleHoverChangesCauseDamage) {
  scene::Scene<HoverScreen> s{std::in_place};
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(120.0f, 60.0f));
  (void)s.finishFrame();

  s.setHover(20.0f, 7.0f);
  EXPECT_TRUE(s.root().slider.hovered());
  EXPECT_TRUE(s.finishFrame().fDamage.isEmpty());

  s.setHover(20.0f, 35.0f);
  EXPECT_FALSE(s.root().slider.hovered());
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());
}

TEST(TextBox, TextAndSelectionOwnDamage) {
  auto made = sceneOf({.width = 100.0f, .height = 30.0f},
                      widgets::TextBox<>("Size"));
  auto &s = *made;
  auto &box = s.root().child;
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(120.0f, 50.0f));
  (void)s.finishFrame();

  box.setText("1920x1080");
  EXPECT_EQ(box.text(), "1920x1080");
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());

  box.setSelected(true);
  EXPECT_TRUE(box.selected());
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());
}

TEST(TextBox, MaskedTextStaysOutOfTheSemantics) {
  auto made = sceneOf({.width = 100.0f, .height = 30.0f},
                      widgets::TextBox<>("Password"));
  auto &s = *made;
  auto &box = s.root().child;
  box.setMasked(true);
  box.setText("secret");
  s.layoutIfNeeded(skia::SkRect::MakeWH(120.0f, 50.0f));
  const auto tree = s.semanticsTree();
  ASSERT_EQ(tree.size(), 1u);
  EXPECT_TRUE(tree[0].fValue.empty());
  EXPECT_EQ(box.text(), "secret");
}

TEST(RangeSlider, RoutedDragKeepsCaptureOutsideItsBounds) {
  auto made = sceneOf({.x = 10.0f,
                       .y = 13.0f,
                       .width = 100.0f,
                       .height = 14.0f,
                       .relativeSize = scene::axes::kNone},
                      widgets::RangeSlider([](float, float) {}));
  auto &s = *made;
  auto &slider = s.root().child;
  slider.setMinimumSpan(0.1f);
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(200.0f, 40.0f));

  scene::PointerEvent down = scene::pointer::down{60.0f, 20.0f};
  EXPECT_TRUE(s.dispatchPointer(down));
  EXPECT_EQ(s.capturedId(), slider.id());
  EXPECT_FLOAT_EQ(slider.low(), 0.5f);

  scene::PointerEvent move = scene::pointer::move{200.0f, 100.0f};
  EXPECT_TRUE(s.dispatchPointer(move));
  EXPECT_NEAR(slider.low(), 0.9f, 0.0001f);

  scene::PointerEvent up = scene::pointer::up{200.0f, 100.0f};
  EXPECT_TRUE(s.dispatchPointer(up));
  EXPECT_EQ(s.capturedId(), 0u);
  EXPECT_FALSE(slider.dragging());
}

TEST(Button, TabFocusAndEnterActivate) {
  int clicks = 0;
  const auto apply = [&clicks] { ++clicks; };
  auto made = sceneOf({.width = 100.0f, .height = 30.0f},
                      widgets::Button<decltype(apply)>("Apply", apply));
  auto &s = *made;
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(120.0f, 50.0f));
  (void)s.finishFrame();

  EXPECT_TRUE(s.dispatchKey(scene::key::down{scene::keys::kTab}));
  EXPECT_EQ(s.focusedId(), s.root().child.id());
  EXPECT_FALSE(s.finishFrame().fDamage.isEmpty());

  EXPECT_TRUE(s.dispatchKey(scene::key::down{scene::keys::kEnter}));
  EXPECT_EQ(clicks, 1);
}

TEST(TextBox, RoutedUtf8AndCompositionUseFocus) {
  std::string changed;
  const auto typed = [&changed](std::string_view text) { changed = text; };
  auto made = sceneOf(
      {.width = 100.0f, .height = 30.0f},
      widgets::TextBox<decltype(typed)>("Search", typed));
  auto &s = *made;
  auto &box = s.root().child;
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(120.0f, 50.0f));

  scene::PointerEvent down = scene::pointer::down{10.0f, 10.0f};
  EXPECT_TRUE(s.dispatchPointer(down));
  EXPECT_EQ(s.focusedId(), box.id());

  EXPECT_TRUE(s.dispatchText(scene::text::compose{"ka"}));
  EXPECT_TRUE(box.text().empty());

  EXPECT_TRUE(s.dispatchText(scene::text::commit{"か"}));
  EXPECT_EQ(box.text(), "か");
  EXPECT_EQ(changed, "か");

  EXPECT_TRUE(s.dispatchKey(scene::key::down{scene::keys::kBackspace}));
  EXPECT_TRUE(box.text().empty());

  const auto semantics = s.semanticsTree();
  ASSERT_EQ(semantics.size(), 1u);
  EXPECT_TRUE(scene::isTextBox(semantics[0].fRole));
  EXPECT_TRUE(semantics[0].fFocused);
}

struct Counted {
  int *clicks;
  void operator()() const { ++*clicks; }
};
struct Stored {
  float *value;
  void operator()(float v) const { *value = v; }
};
struct AccessibleScreen : scene::Node {
  widgets::Button<Counted> button;
  widgets::SliderBar<Stored> slider;
  widgets::TextBox<> box;
  AccessibleScreen(int *clicks, float *value)
      : button(scene::make<widgets::Button<Counted>>(
            {.width = 100.0f, .height = 30.0f}, "Apply", Counted{clicks})),
        slider(scene::make<widgets::SliderBar<Stored>>(
            {.y = 35.0f, .width = 100.0f, .height = 14.0f}, Stored{value})),
        box(scene::make<widgets::TextBox<>>(
            {.y = 55.0f, .width = 100.0f, .height = 30.0f}, "Search")) {}
  void forEachChild(auto &&f) {
    f(button);
    f(slider);
    f(box);
  }
};

TEST(Accessibility, SemanticActionsOperateWidgets) {
  int clicks = 0;
  float sliderValue = 0.0f;
  scene::Scene<AccessibleScreen> s{std::in_place, &clicks, &sliderValue};
  s.state().apply({.fill = true});
  s.layoutIfNeeded(skia::SkRect::MakeWH(120.0f, 100.0f));

  const auto tree = s.semanticsTree();
  ASSERT_EQ(tree.size(), 3u);
  EXPECT_TRUE(s.dispatchSemantic(tree[0].fId, scene::semantic_action::activate{}));
  EXPECT_EQ(clicks, 1);

  EXPECT_TRUE(s.dispatchSemantic(tree[1].fId, scene::semantic_action::set_value{0.75f}));
  EXPECT_FLOAT_EQ(sliderValue, 0.75f);

  EXPECT_TRUE(s.dispatchSemantic(tree[2].fId, scene::semantic_action::set_value{0.0f, "artist"}));
  EXPECT_EQ(s.root().box.text(), "artist");
}


// The base changes independently of the closed drawer: a composer relayout
// or a scrolling list must not repaint the full window-sized wrapper.
auto drawerScene() {
  using Box = skiff::nodes::Box<>;
  using Base = skiff::nodes::Box<Box>;
  using Drawer = widgets::Drawer<Base, Box>;
  auto made = std::make_unique<scene::Scene<Drawer>>(
      std::in_place, std::piecewise_construct,
      std::make_tuple(skia::SkColor{0xFF202020},
                      scene::make<Box>({.width = 40.0f, .height = 20.0f}, skia::SkColor{0xFFFFFFFF})),
      std::make_tuple(skia::SkColor{0xFF303030}));
  made->root().base().apply({.fill = true});
  made->layoutIfNeeded(skia::SkRect::MakeWH(800.0f, 600.0f));
  (void)made->finishFrame();
  return made;
}

TEST(Drawer, ClosedBaseRelayoutKeepsDamageLocal) {
  auto made = drawerScene();
  auto& child = std::get<0>(made->root().base().fChildren);
  for (int frame = 0; frame < 3; ++frame) {
    child.invalidateLayout();
    made->layoutIfNeeded(skia::SkRect::MakeWH(800.0f, 600.0f));
    const auto damage = made->finishFrame();
    EXPECT_EQ(damage.fDamage, child.bounds());
  }
}

TEST(Drawer, AnimatedBasePaintKeepsDamageLocal) {
  auto made = drawerScene();
  auto& child = std::get<0>(made->root().base().fChildren);
  for (int frame = 0; frame < 3; ++frame) {
    child.markDamaged();
    // Another changing child can require the base to lay out in this frame.
    made->root().base().fState.relayoutQuietly();
    made->layoutIfNeeded(skia::SkRect::MakeWH(800.0f, 600.0f));
    EXPECT_EQ(made->finishFrame().fDamage, child.bounds());
  }
}

TEST(Drawer, OpeningStillRepaintsTheScrim) {
  auto made = drawerScene();
  made->root().open();
  made->update(0.0);
  made->layoutIfNeeded(skia::SkRect::MakeWH(800.0f, 600.0f));
  (void)made->finishFrame();
  made->update(100.0);
  made->layoutIfNeeded(skia::SkRect::MakeWH(800.0f, 600.0f));
  EXPECT_TRUE(made->finishFrame().fDamage.contains(skia::SkRect::MakeWH(800.0f, 600.0f)));
}

} // namespace

TEST(ModelSlider, DiscreteChoicesHandleUnknownAndEmptyValues) {
  widgets::ChoiceSliderField<int> slider(widgets::Theme{}, {50, 100, 150});
  slider.read(100);
  EXPECT_FLOAT_EQ(slider.fraction(), 0.5f);
  slider.read(99);
  EXPECT_FLOAT_EQ(slider.fraction(), 0.0f);
  slider.setFraction(0.8f);
  auto picked = slider.onPress();
  ASSERT_TRUE(picked.has_value());
  EXPECT_EQ(picked->fChange.fValue, 150);
  widgets::ChoiceSliderField<int> empty(widgets::Theme{}, {});
  empty.read(100);
  EXPECT_FLOAT_EQ(empty.fraction(), 0.0f);
  EXPECT_FALSE(empty.onPress().has_value());
  widgets::ChoiceSliderField<int> single(widgets::Theme{}, {100});
  single.setFraction(1.0f);
  EXPECT_EQ(single.onPress()->fChange.fValue, 100);
}

TEST(ModelChoice, SelectionAndPressAgreeWithTheModel) {
  widgets::ChoiceRowField<int> row(widgets::Theme{}, "Second", 2);
  row.read(1);
  EXPECT_FALSE(row.semantics().fSelected);
  row.read(2);
  EXPECT_TRUE(row.semantics().fSelected);
  EXPECT_EQ(row.semantics().fLabel, "Second");
  EXPECT_EQ(row.onPress().fChange.fValue, 2);
}

TEST(ModelButton, BoundEventAndLabelFollowModelEdits) {
  struct event { int value; };
  widgets::SendButton<event> button(widgets::Theme{}, "First", event{1});
  button.read(std::pair{std::string("Second"), event{2}});
  EXPECT_EQ(button.semantics().fLabel, "Second");
  EXPECT_EQ(button.onPress().value, 2);
  button.read(event{3});
  EXPECT_EQ(button.onPress().value, 3);
}
