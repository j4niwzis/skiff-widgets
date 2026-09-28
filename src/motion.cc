export module skiff.widgets.motion;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;

export namespace skiff::widgets {

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

// A base, and over it a panel that slides in from the right and covers it,
// and slides back out when closed -- a drawer, a settings page, a detail
// view, in the one window. The panel sits on a sheet of its own colour, so
// the base does not show through it.
//
// A closed panel is destroyed by dropClosed(), which the program calls
// between events once the slide is over: never inside the panel's own
// handlers. It is a sweeping movement: below skiff::paint::motion::full it
// comes and goes at once.
template <class Base, class Over>
class SlideOver : public skiff::scene::Node {
public:
  template <class... Args>
  explicit SlideOver(Args &&...args) : fBase(std::forward<Args>(args)...) {
    fState.apply({.fill = true});
    fSheet.setVisible(false);
  }

  [[nodiscard]] Base &base() noexcept { return fBase; }
  // The panel that is up, not on its way out.
  [[nodiscard]] Over *shown() noexcept {
    return fOver && !fClosing ? &*fOver : nullptr;
  }
  [[nodiscard]] const Over *shown() const noexcept {
    return fOver && !fClosing ? &*fOver : nullptr;
  }

  void setSheetColour(skia::SkColor colour) { fSheet.setColour(colour); }

  // A panel made from `args`: it slides in, unless one is up already, which
  // it replaces where it is.
  template <class... Args> Over &open(Args &&...args) {
    const bool up = this->shown() != nullptr;
    fOver.emplace(std::forward<Args>(args)...);
    fClosing = false;
    if (!up) {
      fSlide.jump(0.0f);
    }
    fSlide.setTarget(1.0f);
    fSheet.setVisible(true);
    this->invalidateLayout();
    return *fOver;
  }
  // The panel on its way out.
  void close() {
    if (!fOver) {
      return;
    }
    fClosing = true;
    fSlide.setTarget(0.0f);
    this->invalidateLayout();
  }
  // A panel that has slid all the way out, destroyed.
  void dropClosed() {
    if (fOver && fClosing && !fSlide.moving()) {
      fOver.reset();
      fClosing = false;
      fSheet.setVisible(false);
      this->invalidateLayout();
    }
  }

  void forEachChild(auto &&f) {
    f(fBase);
    f(fSheet);
    f(fOver);
  }

  [[nodiscard]] bool settling() const { return fSlide.moving(); }
  void update(double nowMs) {
    if (fSlide.step(nowMs)) {
      this->invalidateLayout();
    }
  }

  void layoutChildren() {
    const skia::SkRect box = fState.contentBox();
    fBase.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fBase, box);
    if (!fOver) {
      return;
    }
    const skia::SkRect over = skia::SkRect::MakeXYWH(
        box.fLeft + box.width() * (1.0f - fSlide.value()), box.fTop,
        box.width(), box.height());
    fSheet.apply({.width = over.width(), .height = over.height()});
    fSheet.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(fSheet, over);
    this->layOut(*fOver, over);
  }

private:
  // The panel itself, or whichever panel a variant holds.
  template <class T> static void layOut(T &panel, const skia::SkRect &area) {
    panel.fState.arrange(0.0f, 0.0f);
    skiff::scene::layout(panel, area);
  }
  template <class... Ts>
  static void layOut(std::variant<Ts...> &panel, const skia::SkRect &area) {
    std::visit([&](auto &one) { layOut(one, area); }, panel);
  }

  Base fBase;
  skiff::nodes::Box<> fSheet{skia::colorSetARGB(255, 0, 0, 0)};
  std::optional<Over> fOver;
  skiff::paint::Eased fSlide{0.0f, 60.0f, skiff::paint::movement::sweeping{}};
  bool fClosing = false;
};

} // namespace skiff::widgets
