export module skiff.widgets.loader;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;
export import skiff.widgets.theme;
export import skiff.widgets.erased;

export namespace skiff::widgets {

// What is loading, as Telegram Desktop's radial loader shows it: a dark disc
// and, spinning in it, a white arc -- as long as the progress, where it is
// known, a short one turning where it is not -- and a cross in the middle to
// stop it. A node of the toolkit's to draw: the program says only how far.
// Pressed, it does `onPress` -- the program stops what is loading, or, where
// it was stopped (an arrow down in the disc then), starts it again.
namespace internal {
template <class OnPress = skiff::scene::NoAction> class RadialLoader : public skiff::scene::Node {
public:
  explicit RadialLoader(float size = 44.0f, OnPress onPress = {}) : fOnPress(std::move(onPress)) {
    fState.apply({.width = size, .height = size, .cornerRadius = size * 0.5f,
                  .background = skia::colorSetARGB(0x54, 0, 0, 0)});
  }
  // How far, 0 to 1; none where it is not known.
  void setProgress(std::optional<float> done) {
    fProgress = done;
    this->markDamaged();
  }
  // Stopped: no arc, an arrow down to start it again.
  void setStopped(bool stopped) {
    if (fStopped != stopped) {
      fStopped = stopped;
      this->markDamaged();
    }
  }
  [[nodiscard]] bool acceptsInput() const { return skiff::scene::acts(fOnPress); }
  [[nodiscard]] bool onClick(float, float) {
    if (!skiff::scene::acts(fOnPress)) {
      return false;
    }
    this->act();
    return true;
  }
  // What a press asks for, where its action answers: asked as the press is
  // delivered.
  auto onPress()
    requires skiff::scene::Answering<OnPress>
  {
    return std::invoke(fOnPress);
  }

private:
  void act()
    requires skiff::scene::Answering<OnPress>
  {
    skiff::scene::pressLater(fState);
  }
  void act() { std::invoke(fOnPress); }

public:
  [[nodiscard]] bool settling() const { return this->visible() && !fStopped; }
  // Turning while it shows; hidden, not ticked -- nor repainted each frame.
  [[nodiscard]] bool wantsTick() const { return fState.fVisible && fState.fAlpha > 0.001f; }
  void update(double nowMs) {
    fAngle = static_cast<float>(std::fmod(nowMs * 0.36, 360.0));  // a turn a second
    // Repainted only while it shows: a hidden one, gone into by a walk that
    // goes everywhere, was repainted at every frame -- an empty square.
    if (this->wantsTick()) {
      this->markDamaged();
    }
  }
  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    const skia::SkRect &box = fState.fBounds;
    const float line = std::max(2.0f, box.width() / 22.0f);
    const float inset = box.width() * 0.14f;
    skia::SkPaint pen;
    pen.setAntiAlias(true);
    pen.setStyle(skia::kStrokeStyle);
    pen.setStrokeWidth(line);
    pen.setStrokeCap(skia::kRoundCap);
    pen.setColor(skia::colorSetARGB(255, 255, 255, 255));
    pen.setAlphaf(alpha);
    if (fStopped) {
      // An arrow down: tdesktop's download mark.
      const float h = box.height() * 0.22f, w = box.width() * 0.14f;
      canvas->drawLine(box.centerX(), box.centerY() - h, box.centerX(), box.centerY() + h, pen);
      canvas->drawLine(box.centerX() - w, box.centerY() + h - w, box.centerX(), box.centerY() + h, pen);
      canvas->drawLine(box.centerX() + w, box.centerY() + h - w, box.centerX(), box.centerY() + h, pen);
      return;
    }
    const float sweep = fProgress ? std::max(12.0f, 360.0f * std::clamp(*fProgress, 0.0f, 1.0f)) : 90.0f;
    canvas->drawArc(box.makeInset(inset, inset), fAngle - 90.0f, sweep, false, pen);
    // The cross.
    const float c = box.width() * 0.13f;
    canvas->drawLine(box.centerX() - c, box.centerY() - c, box.centerX() + c, box.centerY() + c, pen);
    canvas->drawLine(box.centerX() - c, box.centerY() + c, box.centerX() + c, box.centerY() - c, pen);
  }

private:
  std::optional<float> fProgress;
  float fAngle = 0.0f;
  bool fStopped = false;
  [[no_unique_address]] OnPress fOnPress;

public:
  // Where its action keeps the events it sent: taken by a walk that drains
  // them.
  auto takeEvents()
    requires requires(OnPress &a) { a.fEmitted; }
  {
    return std::exchange(fOnPress.fEmitted, {});
  }

private:
};
} // namespace internal

// The loader over AnyAction, taking the action given and erasing it.
template <class OnPress>
class ErasedRadialLoader : public internal::RadialLoader<AnyActionFor<OnPress>> {
  using Base = internal::RadialLoader<AnyActionFor<OnPress>>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  explicit ErasedRadialLoader(float size = 44.0f, OnPress onPress = {})
      : internal::RadialLoader<AnyActionFor<OnPress>>(size, AnyActionFor<OnPress>(std::move(onPress))) {}
};
// The loader: made for its action in a release build, over AnyAction
// otherwise.
template <class OnPress = skiff::scene::NoAction>
using RadialLoader = std::conditional_t<kErasedActions && !KeepsEvents<OnPress>, ErasedRadialLoader<OnPress>, internal::RadialLoader<OnPress>>;


} // namespace skiff::widgets
