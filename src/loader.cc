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
class RadialLoader : public skiff::scene::Node {
public:
  explicit RadialLoader(float size = 44.0f) {
    fState.apply({.width = size, .height = size, .cornerRadius = size * 0.5f,
                  .background = skia::colorSetARGB(0x54, 0, 0, 0)});
  }
  // How far, 0 to 1; none where it is not known.
  void setProgress(std::optional<float> done) {
    fProgress = done;
    this->markDamaged();
  }
  [[nodiscard]] bool settling() const { return this->visible(); }
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
};

} // namespace skiff::widgets
