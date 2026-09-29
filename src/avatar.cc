export module skiff.widgets.avatar;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;
export import skiff.widgets.theme;

export namespace skiff::widgets {

// A round picture of someone or something: its picture where there is one,
// and until then -- or without one -- a gradient with its initials in white.
// AdwAvatar. What the picture is, the initials and the colours are the
// program's to say; the avatar draws nothing by hand.
class Avatar : public skiff::scene::Node {
public:
  struct parts_t {
    skiff::nodes::Text initials;
    skiff::nodes::Image picture;
  } parts;

  Avatar(std::string initials, float size, skiff::nodes::ImageSource picture, skiff::scene::Gradient colours)
      : parts{.initials = skiff::nodes::Text(std::move(initials), size * 0.4f, skia::colorSetARGB(255, 255, 255, 255), true),
              .picture = skiff::nodes::Image(std::move(picture))} {
    fState.apply({.width = size, .height = size, .cornerRadius = size * 0.5f, .gradient = colours, .masking = true});
    parts.initials.apply({.place = skiff::scene::anchor::kCentre});
    parts.picture.apply({.fill = true, .cornerRadius = size * 0.5f});
  }

  // Another's: its initials, its picture and its colours.
  void show(std::string initials, skiff::nodes::ImageSource picture, skiff::scene::Gradient colours) {
    parts.initials.setText(std::move(initials));
    parts.picture.setSource(std::move(picture));
    fState.apply({.gradient = colours});
    this->markDamaged();
  }
};

} // namespace skiff::widgets
