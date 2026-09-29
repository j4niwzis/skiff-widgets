export module skiff.widgets.motion;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;

export namespace skiff::widgets {

// A plate a panel is drawn on: it takes every press on it, so that a press
// on an empty part of the panel does not go through to what is under it.
class Sheet : public skiff::nodes::Box<> {
public:
  explicit Sheet(skia::SkColor colour) : skiff::nodes::Box<>(colour) {}
  [[nodiscard]] bool acceptsInput() const { return true; }
};

// A child that folds open and shut: its height eases between none and the
// child's own, and what does not fit yet is clipped. Shut, the child is
// hidden, so nothing in it takes focus. It is a subtle movement: at
// skiff::paint::motion::none it opens and shuts at once.
//
// The child says its own height (autoSize, a fixed height, or a measure()
// hook); a change in it is followed on the next frame.
template <class Child>
class Collapsible : public skiff::scene::Node {
public:
  template <class... Args>
  explicit Collapsible(Args &&...args) : fChild(std::forward<Args>(args)...) {
    fState.apply({.fillX = true, .masking = true});
    fChild.setVisible(false);
  }

  [[nodiscard]] Child &child() noexcept { return fChild; }
  [[nodiscard]] const Child &child() const noexcept { return fChild; }
  [[nodiscard]] bool isOpen() const noexcept { return fOpen; }

  // Open or shut, eased.
  void setOpen(bool open) {
    fOpen = open;
    fUnfold.setTarget(open ? 1.0f : 0.0f);
    if (open) {
      fChild.setVisible(true);
    }
    this->invalidateLayout();
  }
  // Open or shut, there and then: for the state a screen starts in.
  void setOpenNow(bool open) {
    fOpen = open;
    fUnfold.jump(open ? 1.0f : 0.0f);
    fChild.setVisible(open);
    this->invalidateLayout();
  }

  void forEachChild(auto &&f) { f(fChild); }

  [[nodiscard]] bool settling() const {
    return fUnfold.moving() || fChildHeight != fUsedHeight;
  }
  void update(double nowMs) {
    if (fUnfold.step(nowMs) || fChildHeight != fUsedHeight) {
      this->invalidateLayout();
    }
    if (!fOpen && !fUnfold.moving()) {
      fChild.setVisible(false);
    }
  }

  void measure(const skia::SkRect &) {
    fUsedHeight = fChildHeight;
    fState.fHeight = fChildHeight * fUnfold.value();
  }

  void layoutChildren() {
    const skia::SkRect box = fState.contentBox();
    fChild.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(
        fChild, skia::SkRect::MakeXYWH(box.fLeft, box.fTop, box.width(), kRoom));
    if (fChild.visible()) {
      fChildHeight = fChild.bounds().height();
    }
  }

private:
  // As tall as the child could want to be.
  static constexpr float kRoom = 1.0e6f;

  Child fChild;
  skiff::paint::Eased fUnfold{0.0f, 70.0f, skiff::paint::movement::subtle{}};
  bool fOpen = false;
  // The child's height, as its last layout found it, and the one the last
  // measure() used.
  float fChildHeight = 0.0f;
  float fUsedHeight = 0.0f;
};

// One panel of a SlideOver, on a sheet of its own: it takes every press on
// it, so nothing under it can be reached through it.
template <class Over> class SlideLayer : public skiff::scene::Node {
public:
  template <class... Args>
  explicit SlideLayer(skia::SkColor sheet, Args &&...args)
      : fOver(std::forward<Args>(args)...) {
    fState.apply({.fill = true});
    fSheet.setColour(sheet);
  }

  void forEachChild(auto &&f) {
    f(fSheet);
    f(fOver);
  }
  void layoutChildren() {
    const skia::SkRect box = fState.contentBox();
    fSheet.apply({.width = box.width(), .height = box.height()});
    fSheet.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fSheet, box);
    layOut(fOver, box);
  }
  [[nodiscard]] bool acceptsInput() const { return true; }

  Over fOver;
  skiff::paint::Tween fSlide{0.0f, 220.0f, skiff::paint::movement::sweeping{}};
  bool fClosing = false;

private:
  template <class T> static void layOut(T &panel, const skia::SkRect &area) {
    panel.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(panel, area);
  }
  template <class... Ts>
  static void layOut(std::variant<Ts...> &panel, const skia::SkRect &area) {
    std::visit([&](auto &one) { layOut(one, area); }, panel);
  }

  Sheet fSheet{skia::colorSetARGB(255, 0, 0, 0)};
};

// A base, and over it a stack of panels, each sliding in from the right over
// what is under it and back out when it goes -- a settings page, a detail
// view, a page opened from a page, in the one window. Each panel sits on a
// sheet of its own colour and takes every press on it.
//
// Panels that have gone are destroyed by dropClosed(), which the program
// calls between events once they have slid out: never inside a panel's own
// handlers. It is a sweeping movement: below skiff::paint::motion::full the
// panels come and go at once.
template <class Base, class Over>
class SlideOver : public skiff::scene::Node {
public:
  template <class... Args>
  explicit SlideOver(Args &&...args) : fBase(std::forward<Args>(args)...) {
    fState.apply({.fill = true});
  }

  [[nodiscard]] Base &base() noexcept { return fBase; }
  // The panel on top, of those not on their way out.
  [[nodiscard]] Over *shown() noexcept {
    for (auto it = fLayers.rbegin(); it != fLayers.rend(); ++it) {
      if (!it->fClosing) {
        return &it->fOver;
      }
    }
    return nullptr;
  }

  void setSheetColour(skia::SkColor colour) { fSheetColour = colour; }

  // A panel made from `args`, sliding in over the top one.
  template <class... Args> Over &open(Args &&...args) {
    SlideLayer<Over> &made =
        fLayers.emplace_back(fSheetColour, std::forward<Args>(args)...);
    made.fSlide.jump(0.0f);
    made.fSlide.setTarget(1.0f);
    this->invalidateLayout();
    return made.fOver;
  }
  // The top panel slides out, and the one under it is up again.
  void back() {
    for (auto it = fLayers.rbegin(); it != fLayers.rend(); ++it) {
      if (!it->fClosing) {
        it->fClosing = true;
        it->fSlide.setTarget(0.0f);
        break;
      }
    }
    this->invalidateLayout();
  }
  // Every panel goes: the top one slides out over the base, the ones under
  // it at once.
  void close() {
    bool top = true;
    for (auto it = fLayers.rbegin(); it != fLayers.rend(); ++it) {
      if (it->fClosing) {
        continue;
      }
      it->fClosing = true;
      if (top) {
        it->fSlide.setTarget(0.0f);
        top = false;
      } else {
        it->fSlide.jump(0.0f);
      }
    }
    this->invalidateLayout();
  }
  // The panels that have slid all the way out, destroyed.
  void dropClosed() {
    bool dropped = false;
    while (!fLayers.empty() && fLayers.back().fClosing &&
           !fLayers.back().fSlide.moving()) {
      fLayers.pop_back();
      dropped = true;
    }
    if (dropped) {
      this->invalidateLayout();
    }
  }

  void forEachChild(auto &&f) {
    f(fBase);
    f(fLayers);
  }

  [[nodiscard]] bool settling() const {
    return std::ranges::any_of(fLayers, [](const SlideLayer<Over> &layer) {
      return layer.fSlide.moving();
    });
  }
  void update(double nowMs) {
    bool moved = false;
    for (SlideLayer<Over> &layer : fLayers) {
      moved = layer.fSlide.step(nowMs) || moved;
    }
    if (moved) {
      this->invalidateLayout();
    }
  }

  void layoutChildren() {
    const skia::SkRect box = fState.contentBox();
    fBase.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fBase, box);
    for (SlideLayer<Over> &layer : fLayers) {
      const float value = layer.fSlide.value();
      layer.setVisible(value > 0.0f);
      skiff::scene::layout(
          layer, skia::SkRect::MakeXYWH(box.fLeft + box.width() * (1.0f - value),
                                        box.fTop, box.width(), box.height()));
    }
  }

private:
  Base fBase;
  // A deque: a panel is made in place and never moved.
  std::deque<SlideLayer<Over>> fLayers;
  skia::SkColor fSheetColour = skia::colorSetARGB(255, 0, 0, 0);
};

// A navigation drawer: a panel pulled out from the left edge over a base,
// with the rest of the base dimmed under a scrim. A click on the scrim, or
// Esc from inside the panel, pushes it back. The panel is there all along --
// hidden while pushed in, so nothing in it takes focus -- and draws on a
// sheet of its own colour. It is a sweeping movement: below
// skiff::paint::motion::full it comes and goes at once.
template <class Base, class Content>
class Drawer : public skiff::scene::Node {
public:
  // The base and the panel, each made in place from its own arguments:
  // Drawer(std::piecewise_construct, std::forward_as_tuple(...),
  // std::forward_as_tuple(...)).
  template <class... BaseArgs, class... ContentArgs>
  Drawer(std::piecewise_construct_t, std::tuple<BaseArgs...> base,
         std::tuple<ContentArgs...> content)
      : fBase(std::make_from_tuple<Base>(std::move(base))),
        fContent(std::make_from_tuple<Content>(std::move(content))) {
    fState.apply({.fill = true});
    fScrim.setVisible(false);
    fSheet.setVisible(false);
    fContent.setVisible(false);
  }

  [[nodiscard]] Base &base() noexcept { return fBase; }
  [[nodiscard]] Content &content() noexcept { return fContent; }
  [[nodiscard]] bool isOpen() const noexcept { return fOpen; }

  void setSheetColour(skia::SkColor colour) { fSheet.setColour(colour); }
  // How wide the panel is at most; never more than most of the window.
  void setWidth(float width) {
    fWidth = width;
    this->invalidateLayout();
  }

  void open() { this->setOpen(true); }
  void close() { this->setOpen(false); }
  // Pushed in there and then, with no slide: for when something has just
  // come over it and it goes unseen.
  void closeNow() {
    fOpen = false;
    fSlide.jump(0.0f);
    this->showWhatMoves();
    this->invalidateLayout();
  }
  void setOpen(bool open) {
    fOpen = open;
    fSlide.setTarget(open ? 1.0f : 0.0f);
    this->showWhatMoves();
    this->invalidateLayout();
  }

  void forEachChild(auto &&f) {
    f(fBase);
    f(fScrim);
    f(fSheet);
    f(fContent);
  }

  [[nodiscard]] bool settling() const { return fSlide.moving(); }
  void update(double nowMs) {
    if (fSlide.step(nowMs)) {
      this->showWhatMoves();
      this->invalidateLayout();
    }
  }

  void layoutChildren() {
    const skia::SkRect box = fState.contentBox();
    fBase.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fBase, box);
    if (!fContent.visible()) {
      return;
    }
    const float value = fSlide.value();
    fScrim.setColour(skia::colorSetARGB(static_cast<unsigned>(115.0f * value), 0, 0, 0));
    fScrim.apply({.width = box.width(), .height = box.height()});
    fScrim.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fScrim, box);
    const float width = std::min(fWidth, box.width() * 0.85f);
    const skia::SkRect panel = skia::SkRect::MakeXYWH(
        box.fLeft - width * (1.0f - value), box.fTop, width, box.height());
    fSheet.apply({.width = width, .height = box.height()});
    fSheet.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fSheet, panel);
    fContent.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fContent, panel);
  }

  // A press on the scrim pushes the panel back.
  using Node::onPointer;
  void onPointer(skiff::scene::phase::bubble, const skiff::scene::pointer::down &,
                 skiff::scene::PointerReply &reply) {
    if (fOpen && reply.fTarget == fScrim.id()) {
      this->close();
      reply.handle();
    }
  }
  using Node::onKey;
  void onKey(skiff::scene::phase::bubble, const skiff::scene::key::down &press,
             skiff::scene::Reply &reply) {
    if (fOpen && press.key == skiff::scene::keys::kEscape) {
      this->close();
      reply.handle();
    }
  }

private:
  // What covers the base: it takes the pointer, so a press off the panel
  // closes it rather than reaching what is under it.
  class Scrim : public skiff::nodes::Box<> {
  public:
    Scrim() : skiff::nodes::Box<>(skia::colorSetARGB(0, 0, 0, 0)) {}
    [[nodiscard]] bool acceptsInput() const { return true; }
  };

  // Shown while the panel is out or moving; hidden once it is all the way
  // in.
  void showWhatMoves() {
    const bool shown = fOpen || fSlide.moving();
    fScrim.setVisible(shown);
    fSheet.setVisible(shown);
    fContent.setVisible(shown);
  }

  Base fBase;
  Scrim fScrim;
  Sheet fSheet{skia::colorSetARGB(255, 0, 0, 0)};
  Content fContent;
  skiff::paint::Tween fSlide{0.0f, 220.0f, skiff::paint::movement::sweeping{}};
  float fWidth = 300.0f;
  bool fOpen = false;
};

// A box in the middle of the window over a dimmed background, as a settings
// or confirmation dialog: it fades in, and a press off it or Esc from inside
// it fades it out. It is a layer of its own -- it fills its parent and goes
// last among the parent's children -- and holds nothing while shut. The
// program destroys a shut one with dropClosed(), between events. It is a
// subtle movement: at skiff::paint::motion::none it comes and goes at once.
template <class Content>
class Dialog : public skiff::scene::Node {
public:
  Dialog() {
    fState.apply({.fill = true});
    fScrim.setVisible(false);
    fSheet.setVisible(false);
    fSheet.apply({.cornerRadius = 12.0f});
  }

  void setSheetColour(skia::SkColor colour) { fSheet.setColour(colour); }
  // How big the box is at most; never more than most of the window.
  void setSize(float width, float height) {
    fWidth = width;
    fHeight = height;
    this->invalidateLayout();
  }

  // The content up, not on its way out.
  [[nodiscard]] Content *shown() noexcept {
    return fContent && !fClosing ? &*fContent : nullptr;
  }

  // Content made from `args`, in place of any that is up.
  template <class... Args> Content &open(Args &&...args) {
    const bool up = this->shown() != nullptr;
    fContent.emplace(std::forward<Args>(args)...);
    fClosing = false;
    if (!up) {
      fFade.jump(0.0f);
    }
    fFade.setTarget(1.0f);
    this->showFade();
    this->invalidateLayout();
    return *fContent;
  }
  void close() {
    if (!fContent) {
      return;
    }
    fClosing = true;
    fFade.setTarget(0.0f);
    this->showFade();
  }
  void dropClosed() {
    if (fContent && fClosing && !fFade.moving()) {
      fContent.reset();
      fClosing = false;
      fScrim.setVisible(false);
      fSheet.setVisible(false);
      this->invalidateLayout();
    }
  }

  void forEachChild(auto &&f) {
    f(fScrim);
    f(fSheet);
    f(fContent);
  }

  [[nodiscard]] bool settling() const { return fFade.moving(); }
  void update(double nowMs) {
    if (fFade.step(nowMs)) {
      this->showFade();
    }
  }

  void layoutChildren() {
    if (!fContent) {
      return;
    }
    const skia::SkRect box = fState.contentBox();
    fScrim.apply({.width = box.width(), .height = box.height()});
    fScrim.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fScrim, box);
    const float width = std::min(fWidth, box.width() * 0.92f);
    const float height = std::min(fHeight, box.height() * 0.9f);
    const skia::SkRect area = skia::SkRect::MakeXYWH(
        box.centerX() - width * 0.5f, box.centerY() - height * 0.5f, width, height);
    fSheet.apply({.width = width, .height = height});
    fSheet.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fSheet, area);
    fContent->fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(*fContent, area);
  }

  using Node::onPointer;
  void onPointer(skiff::scene::phase::bubble, const skiff::scene::pointer::down &,
                 skiff::scene::PointerReply &reply) {
    if (this->shown() != nullptr && reply.fTarget == fScrim.id()) {
      this->close();
      reply.handle();
    }
  }
  using Node::onKey;
  void onKey(skiff::scene::phase::bubble, const skiff::scene::key::down &press,
             skiff::scene::Reply &reply) {
    if (this->shown() != nullptr && press.key == skiff::scene::keys::kEscape) {
      this->close();
      reply.handle();
    }
  }

private:
  class Scrim : public skiff::nodes::Box<> {
  public:
    Scrim() : skiff::nodes::Box<>(skia::colorSetARGB(115, 0, 0, 0)) {}
    [[nodiscard]] bool acceptsInput() const { return true; }
  };

  void showFade() {
    const bool shown = fContent.has_value();
    fScrim.setVisible(shown);
    fSheet.setVisible(shown);
    const float value = fFade.value();
    fScrim.fState.setAlpha(value);
    fSheet.fState.setAlpha(value);
    if (fContent) {
      fContent->fState.setAlpha(value);
    }
  }

  Scrim fScrim;
  Sheet fSheet{skia::colorSetARGB(255, 0, 0, 0)};
  std::optional<Content> fContent;
  skiff::paint::Tween fFade{0.0f, 160.0f, skiff::paint::movement::subtle{}};
  float fWidth = 420.0f;
  float fHeight = 560.0f;
  bool fClosing = false;
};

} // namespace skiff::widgets
