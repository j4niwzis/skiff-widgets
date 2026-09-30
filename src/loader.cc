export module skiff.widgets.loader;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;
export import skiff.widgets.theme;

export namespace skiff::widgets {

// What is loading, as Telegram Desktop's radial loader shows it: a dark disc
// and, spinning in it, a white arc -- as long as the progress, where it is
// known, a short one turning where it is not -- and a cross in the middle to
// stop it. A node of the toolkit's to draw: the program says only how far.
// Pressed, it does `onPress` -- the program stops what is loading, or, where
// it was stopped (an arrow down in the disc then), starts it again.
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
    std::invoke(fOnPress);
    return true;
  }
  [[nodiscard]] bool settling() const { return this->visible() && !fStopped; }
  // Turning while it shows; hidden, not ticked -- nor repainted each frame.
  [[nodiscard]] bool wantsTick() const { return fState.fVisible && fState.fAlpha > 0.001f; }
  void update(double nowMs) {
    fAngle = static_cast<float>(std::fmod(nowMs * 0.36, 360.0));  // a turn a second
    this->markDamaged();
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
};

} // namespace skiff::widgets
