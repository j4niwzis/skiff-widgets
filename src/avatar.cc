export module skiff.widgets.avatar;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;
export import skiff.widgets.theme;
export import skiff.widgets.erased;

export namespace skiff::widgets {

// A round picture of someone or something: its picture where there is one,
// and until then -- or without one -- a gradient with its initials in white.
// AdwAvatar. What the picture is, the initials and the colours are the
// program's to say; the avatar draws nothing by hand.
namespace internal {
template <skiff::nodes::ImageSource Source> class Avatar : public skiff::scene::Node {
public:
  struct parts_t {
    skiff::nodes::Text initials;
    skiff::nodes::internal::Image<Source> picture;
  } parts;

  Avatar(std::string initials, float size, Source picture, skiff::scene::Gradient colours)
      : parts{.initials = skiff::nodes::Text(std::move(initials), size * 0.4f, skia::colorSetARGB(255, 255, 255, 255), true),
              .picture = skiff::nodes::internal::Image<Source>(std::move(picture))},
        fColours(colours) {
    fState.apply({.width = size, .height = size, .cornerRadius = size * 0.5f, .gradient = colours, .masking = true});
    parts.initials.apply({.place = skiff::scene::anchor::kCentre});
    parts.picture.apply({.fill = true, .cornerRadius = size * 0.5f});
    // The avatar's box, whatever the picture: its coming repaints it and lays
    // nothing out -- every new row of a list laid the list out again as its
    // avatar came, in the middle of the list sliding in.
    parts.picture.keepBox();
    this->showFallback(parts.picture.image() == nullptr);
  }

  // Another's: its initials, its picture and its colours.
  void show(std::string initials, Source picture, skiff::scene::Gradient colours) {
    parts.initials.setText(std::move(initials));
    parts.picture.setSource(std::move(picture));
    fColours = colours;
    fFallback.reset();
    this->showFallback(parts.picture.image() == nullptr);
    this->markDamaged();
  }

  // The picture come, or let go by its cache: the gradient and the initials
  // under it only while there is none -- seen through an avatar drawn at
  // an opacity, they were a second avatar under the first.
  void update(double) { this->showFallback(parts.picture.image() == nullptr); }
  // And as it is drawn: a picture found at a repaint takes the fallback away.
  void drawSelf(skia::SkCanvas *, float) { this->showFallback(parts.picture.image() == nullptr); }

private:
  void showFallback(bool shown) {
    if (fFallback == shown) {
      return;
    }
    fFallback = shown;
    parts.initials.setVisible(shown);
    fState.apply({.gradient = shown ? fColours : skiff::scene::Gradient{}});
    this->markDamaged();
  }
  skiff::scene::Gradient fColours;
  std::optional<bool> fFallback;

public:
};
} // namespace internal

// The avatar over AnyImageSource, taking a picture of its own type and
// erasing it: all of its code but this is internal::Avatar<AnyImageSource>'s.
template <skiff::nodes::ImageSource Source>
class ErasedAvatar : public internal::Avatar<skiff::nodes::AnyImageSource> {
  using Base = internal::Avatar<skiff::nodes::AnyImageSource>;

public:
  ErasedAvatar(std::string initials, float size, Source picture, skiff::scene::Gradient colours)
      : Base(std::move(initials), size, skiff::nodes::AnyImageSource(std::move(picture)), colours) {}
  void show(std::string initials, Source picture, skiff::scene::Gradient colours) {
    Base::show(std::move(initials), skiff::nodes::AnyImageSource(std::move(picture)), colours);
  }
};
// The avatar: made for its picture in a release build, over AnyImageSource
// otherwise.
template <skiff::nodes::ImageSource Source>
using Avatar = std::conditional_t<kErasedActions, ErasedAvatar<Source>, internal::Avatar<Source>>;


} // namespace skiff::widgets
