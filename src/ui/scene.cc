// skiff.ui on the scene: the one vocabulary a program writes its UI in,
// answered here by skiff.compose and skiff-widgets, drawn through Skia. The
// same module name is answered by src/ui/web.cc for a page in a browser;
// the build picks one (SKIFF_UI_BACKEND), and the program is written once.
export module skiff.ui;

export import skiff.core;
export import skiff.model;
export import skiff.bind;
export import skiff.scene;
export import skiff.nodes;
export import skiff.compose;
export import skiff.widgets;

import std;

export namespace skiff::ui {

// Stacks and the model's kinds around a node, as skiff.compose spells them.
using skiff::compose::Look;
using skiff::compose::vbox;
using skiff::compose::hbox;
using skiff::compose::column;
using skiff::compose::row;
using skiff::compose::bound;
using skiff::compose::scoped;
using skiff::compose::local;
using skiff::compose::handle;
using skiff::compose::handlers;
using skiff::compose::Named;
// A node drawn by its own hand, as the page has one.
using skiff::nodes::Canvas;
// Shapes as nodes in a figure, and a list of them: as the page has them.
using skiff::nodes::Figure;
using skiff::nodes::figure;
using skiff::nodes::Polygon;
using skiff::nodes::Polyline;
using skiff::nodes::Line;
using skiff::nodes::Circle;
using skiff::compose::each;
using skiff::compose::group;

// The controls, bound to a model, as skiff-widgets spells them.
using skiff::widgets::Theme;
using skiff::widgets::theme;
using skiff::widgets::ToggleField;
using skiff::widgets::SliderField;
using skiff::widgets::TextField;
using skiff::widgets::ChoiceTabs;
using skiff::widgets::ChoiceList;
using skiff::widgets::SendButton;

// A text and a box said by a Spec alone, as the page says them: the font
// and the colour the theme's, the box's colour its background.
class Text : public skiff::nodes::Text {
public:
  explicit Text(std::string text, skiff::scene::Spec spec = {})
      : skiff::nodes::Text(std::move(text), theme().fFontSize, theme().fLabel) {
    this->apply(spec);
  }
};
struct Box : skiff::nodes::Box<> {
  explicit Box(skiff::scene::Spec spec = {}) : skiff::nodes::Box<>(spec.background.value_or(0u)) { this->apply(spec); }
};

} // namespace skiff::ui
