export module skiff.widgets.pill;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;
export import skiff.widgets.theme;

export namespace skiff::widgets {

// A short label on a rounded plate: a count, a tag, a hint -- as wide as it
// says, its plate's radius half its height. What colours and how big are
// the program's to say.
class Pill : public skiff::nodes::Stack {
public:
  struct parts_t {
    skiff::nodes::Text label;
  } parts;

  struct Look {
    skia::SkColor plate = 0;
    skia::SkColor text = skia::colorSetARGB(255, 255, 255, 255);
    float size = 12.0f;  // the text's
    float height = 20.0f;
    float padX = 8.0f;
    bool bold = false;
  };

  Pill(std::string label, Look look) : parts{.label = skiff::nodes::Text(std::move(label), look.size, look.text, look.bold)} {
    fStack.justify = skiff::nodes::justify::middle{};
    this->setHorizontal();
    fState.apply({.height = look.height,
                  .autoSize = skiff::scene::axes::kX,
                  .minWidth = look.height,
                  .padding = {0.0f, look.padX, 0.0f, look.padX},
                  .cornerRadius = look.height * 0.5f,
                  .background = look.plate});
    parts.label.apply({.alignSelf = skiff::scene::align::kMiddle});
  }

  void setLabel(std::string label) {
    parts.label.setText(std::move(label));
    this->invalidateLayout();
  }
  // Other colours: a pill chosen, muted.
  void setColours(skia::SkColor plate, skia::SkColor text) {
    fState.apply({.background = plate});
    parts.label.setColour(text);
    this->markDamaged();
  }
};

} // namespace skiff::widgets
