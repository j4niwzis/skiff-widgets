export module skiff.widgets.sliderbar;

import std;
import skia;
import skiff.paint;
import skiff.scene;
export import skiff.widgets.theme;

namespace skiff::widgets {
using skiff::scene::Margin;
using skiff::scene::Spec;
} // namespace skiff::widgets

export namespace skiff::widgets {

// A track with a filled part and a knob on the join. The value is a fraction
// of the width, so the bar never sees the units it stands for.
//
// Dragging is not handled here: a drag starts on this and continues wherever
// the pointer goes, which is the screen's business. fractionAt turns a
// pointer position into a value using the bar's own bounds, which is the part
// the screen would otherwise work out from a rectangle it kept a copy of.
template <class OnSet = skiff::scene::NoAction>
class SliderBar : public skiff::scene::Node {
public:
  explicit SliderBar(OnSet onSet = {}) : fOnSet(std::move(onSet)) {
    fState.fRelativeSizeAxes = skiff::scene::axes::kX;
    fState.fWidth = 1.0f;
    fState.fHeight = 6.0f;
  }

  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }

  void setFraction(float fraction) {
    const float clamped = std::clamp(fraction, 0.0f, 1.0f);
    if (clamped == fFraction) {
      return;
    }
    fFraction = clamped;
    this->markDamaged();
  }
  [[nodiscard]] float fraction() const noexcept { return fFraction; }

  // Where along the track a pointer at x sits, as a fraction.
  [[nodiscard]] float fractionAt(float x) const {
    if (fState.fBounds.width() <= 0.0f) {
      return fFraction;
    }
    return std::clamp((x - fState.fBounds.fLeft) / fState.fBounds.width(), 0.0f, 1.0f);
  }

public:
  Theme fTheme = theme();
  float fKnobRadius = 7.0f;
  float fTrackRadius = 3.0f;

  // The knob stands proud of the track, so the box that takes a click is
  // taller than the box that is drawn.
  [[nodiscard]] skia::SkRect reach() const {
    return skia::SkRect::MakeLTRB(
        fState.fBounds.fLeft, fState.fBounds.centerY() - fKnobRadius, fState.fBounds.fRight,
        fState.fBounds.centerY() + fKnobRadius);
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    p.fillRounded(fState.fBounds, fTrackRadius, fTheme.fSurface, alpha);
    p.fillRounded(skia::SkRect::MakeXYWH(fState.fBounds.fLeft, fState.fBounds.fTop,
                                         fState.fBounds.width() * fFraction,
                                         fState.fBounds.height()),
                  fTrackRadius, fTheme.fAccent, alpha);
    p.circle(fState.fBounds.fLeft + fState.fBounds.width() * fFraction, fState.fBounds.centerY(),
             fKnobRadius, fTheme.fText, alpha);
    if (this->focused()) {
      p.strokeRounded(this->reach(), fKnobRadius, fTheme.fAccent, 1.5f,
                      alpha);
    }
  }

  // Without a setter it is a picture of a value and clicks fall through to
  // whatever is behind it.
  [[nodiscard]] bool acceptsInput() const { return skiff::scene::acts(fOnSet); }
  [[nodiscard]] bool focusChangesAppearance() const {
    return skiff::scene::acts(fOnSet);
  }

  using Node::onPointer;
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::down &press,
                 skiff::scene::PointerReply &reply) {
    if (!skiff::scene::acts(fOnSet) || !this->reach().contains(press.x, press.y)) {
      return;
    }
    fDragging = true;
    std::invoke(fOnSet, this->fractionAt(press.x));
    reply.capturePointer();
    reply.requestFocus();
    reply.handle();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::move &move,
                 skiff::scene::PointerReply &reply) {
    if (fDragging) {
      std::invoke(fOnSet, this->fractionAt(move.x));
      reply.handle();
    }
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::up &,
                 skiff::scene::PointerReply &reply) {
    this->stopDragging(reply);
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::cancel &,
                 skiff::scene::PointerReply &reply) {
    this->stopDragging(reply);
  }
  void stopDragging(skiff::scene::PointerReply &reply) {
    if (fDragging) {
      fDragging = false;
      reply.releasePointer();
      reply.handle();
    }
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::slider{};
    out.fValue = std::format("{:.3f}", fFraction);
    out.fActions = {skiff::scene::semantic_action::focus{},
                    skiff::scene::semantic_action::increment{},
                    skiff::scene::semantic_action::decrement{},
                    skiff::scene::semantic_action::set_value{}};
    return out;
  }

  using Node::onSemantic;
  void onSemantic(skiff::scene::phase::target, const skiff::scene::semantic_action::increment &,
                  skiff::scene::Reply &reply) {
    this->setBy(std::min(1.0f, fFraction + 0.05f), reply);
  }
  void onSemantic(skiff::scene::phase::target, const skiff::scene::semantic_action::decrement &,
                  skiff::scene::Reply &reply) {
    this->setBy(std::max(0.0f, fFraction - 0.05f), reply);
  }
  void onSemantic(skiff::scene::phase::target, const skiff::scene::semantic_action::set_value &set,
                  skiff::scene::Reply &reply) {
    this->setBy(std::clamp(set.value, 0.0f, 1.0f), reply);
  }
  void setBy(float fraction, skiff::scene::Reply &reply) {
    if (skiff::scene::acts(fOnSet)) {
      std::invoke(fOnSet, fraction);
      reply.handle();
    }
  }

  [[nodiscard]] bool onClick(float x, float y) {
    if (!skiff::scene::acts(fOnSet) || !this->reach().contains(x, y)) {
      return false;
    }
    if (skiff::scene::acts(fOnSet)) {
      std::invoke(fOnSet, this->fractionAt(x));
    }
    return true;
  }

private:
  [[no_unique_address]] OnSet fOnSet;
  float fFraction = 0.0f;
  bool fDragging = false;
};
SliderBar() -> SliderBar<>;

// A track with two independently draggable ends. Values stay normalised so
// the widget can represent difficulty, price, time or any other range without
// knowing its units. It owns handle selection and pointer-to-track mapping;
// the screen only forwards a continuing drag after the initial click.
template <class OnSet = skiff::scene::NoAction>
class RangeSlider : public skiff::scene::Node {
public:
  explicit RangeSlider(OnSet onSet = {}) : fOnSet(std::move(onSet)) {
    fState.fRelativeSizeAxes = skiff::scene::axes::kX;
    fState.fWidth = 1.0f;
    // The track is six pixels high, but a seven-pixel-radius knob is the
    // actual interaction target. Scene hit testing uses the drawable bounds,
    // so the bounds describe the whole control rather than only its track.
    fState.fHeight = 14.0f;
  }

  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }
  void setMinimumSpan(float span) {
    span = std::clamp(span, 0.0f, 1.0f);
    if (span != fMinSpan) {
      fMinSpan = span;
      this->setRange(fLow, fHigh);
    }
  }

  void setRange(float low, float high) {
    low = std::clamp(low, 0.0f, 1.0f);
    high = std::clamp(high, 0.0f, 1.0f);
    if (low > high) {
      std::swap(low, high);
    }
    const float span = std::clamp(fMinSpan, 0.0f, 1.0f);
    if (high - low < span) {
      high = std::min(1.0f, low + span);
      low = std::max(0.0f, high - span);
    }
    if (low == fLow && high == fHigh) {
      return;
    }
    fLow = low;
    fHigh = high;
    this->markDamaged();
  }

  [[nodiscard]] float low() const noexcept { return fLow; }
  [[nodiscard]] float high() const noexcept { return fHigh; }
  [[nodiscard]] bool dragging() const noexcept { return fDragging >= 0; }

  [[nodiscard]] float fractionAt(float x) const {
    if (fState.fBounds.width() <= 0.0f) {
      return fDragging == 0 ? fLow : fHigh;
    }
    return std::clamp((x - fState.fBounds.fLeft) / fState.fBounds.width(), 0.0f, 1.0f);
  }

  void dragTo(float x) {
    if (fDragging < 0) {
      return;
    }
    const float at = this->fractionAt(x);
    const float span = std::clamp(fMinSpan, 0.0f, 1.0f);
    if (fDragging == 0) {
      this->change(std::min(at, fHigh - span), fHigh);
    } else {
      this->change(fLow, std::max(at, fLow + span));
    }
  }

  void endDrag() noexcept { fDragging = -1; }

public:
  Theme fTheme = theme();
  float fKnobRadius = 7.0f;
  float fTrackHeight = 6.0f;
  float fTrackRadius = 3.0f;
  float fMinSpan = 0.0f;

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    const float trackHeight = std::min(fTrackHeight, fState.fBounds.height());
    const skia::SkRect track = skia::SkRect::MakeXYWH(
        fState.fBounds.fLeft, fState.fBounds.centerY() - trackHeight * 0.5f,
        fState.fBounds.width(), trackHeight);
    p.fillRounded(track, fTrackRadius, fTheme.fSurface, alpha);
    p.fillRounded(
        skia::SkRect::MakeLTRB(track.fLeft + track.width() * fLow,
                               track.fTop,
                               track.fLeft + track.width() * fHigh,
                               track.fBottom),
        fTrackRadius, fTheme.fAccent, alpha);
    p.circle(track.fLeft + track.width() * fLow, track.centerY(),
             fKnobRadius, fTheme.fText, alpha);
    p.circle(track.fLeft + track.width() * fHigh, track.centerY(),
             fKnobRadius, fTheme.fText, alpha);
    if (this->focused()) {
      p.strokeRounded(fState.fBounds, fKnobRadius, fTheme.fAccent, 1.5f, alpha);
    }
  }

  [[nodiscard]] bool acceptsInput() const { return skiff::scene::acts(fOnSet); }
  [[nodiscard]] bool focusChangesAppearance() const {
    return skiff::scene::acts(fOnSet);
  }

  using Node::onPointer;
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::down &press,
                 skiff::scene::PointerReply &reply) {
    if (!skiff::scene::acts(fOnSet) || !fState.fBounds.contains(press.x, press.y)) {
      return;
    }
    (void)this->onClick(press.x, press.y);
    reply.capturePointer();
    reply.requestFocus();
    reply.handle();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::move &move,
                 skiff::scene::PointerReply &reply) {
    if (this->dragging()) {
      this->dragTo(move.x);
      reply.handle();
    }
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::up &,
                 skiff::scene::PointerReply &reply) {
    this->stopDragging(reply);
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::cancel &,
                 skiff::scene::PointerReply &reply) {
    this->stopDragging(reply);
  }
  void stopDragging(skiff::scene::PointerReply &reply) {
    if (this->dragging()) {
      this->endDrag();
      reply.releasePointer();
      reply.handle();
    }
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::slider{};
    out.fValue = std::format("{:.3f}–{:.3f}", fLow, fHigh);
    out.fActions = {skiff::scene::semantic_action::focus{}};
    return out;
  }

  [[nodiscard]] bool onClick(float x, float) {
    if (!skiff::scene::acts(fOnSet)) {
      return false;
    }
    const float at = this->fractionAt(x);
    fDragging = std::abs(at - fLow) <= std::abs(at - fHigh) ? 0 : 1;
    this->dragTo(x);
    return true;
  }

private:
  void change(float low, float high) {
    low = std::clamp(low, 0.0f, 1.0f);
    high = std::clamp(high, 0.0f, 1.0f);
    if (low == fLow && high == fHigh) {
      return;
    }
    fLow = low;
    fHigh = high;
    this->markDamaged();
    if (skiff::scene::acts(fOnSet)) {
      std::invoke(fOnSet, fLow, fHigh);
    }
  }

  [[no_unique_address]] OnSet fOnSet;
  float fLow = 0.0f;
  float fHigh = 1.0f;
  int fDragging = -1;
};
RangeSlider() -> RangeSlider<>;

// A pill that slides its knob from one end to the other. The state is set
// from outside; what is animated here is only the knob catching up with it.
template <class OnToggle = skiff::scene::NoAction>
class Toggle : public skiff::scene::Node {
public:
  explicit Toggle(OnToggle onToggle = {}) : fOnToggle(std::move(onToggle)) {
    fState.fWidth = 40.0f;
    fState.fHeight = 22.0f;
  }

  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }

  // On or off there and then, the knob already where it goes: for the
  // state a screen opens in, where setOn() would show it sliding.
  void setOnNow(bool on) {
    fOn = on;
    fKnob = on ? 1.0f : 0.0f;
    this->markDamaged();
  }
  void setOn(bool on) {
    if (on == fOn) {
      return;
    }
    fOn = on;
    this->markDamaged();
  }
  [[nodiscard]] bool on() const noexcept { return fOn; }

public:
  Theme fTheme = theme();
  float fKnobRadius = 8.0f;
  float fKnobInset = 11.0f;
  float fTauMs = 60.0f;

  [[nodiscard]] bool settling() const {
    return !skiff::paint::settled(fKnob, fOn ? 1.0f : 0.0f);
  }

  void update(double nowMs) {
    const double dt = fLastMs > 0.0 ? nowMs - fLastMs : 16.0;
    fLastMs = nowMs;
    const float previous = fKnob;
    fKnob = skiff::paint::approach(fKnob, fOn ? 1.0f : 0.0f, fTauMs, dt);
    if (fKnob != previous) {
      this->markDamaged();
    }
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    p.fillRounded(fState.fBounds, fState.fBounds.height() * 0.5f,
                  mix(fTheme.fSurface, fTheme.fAccent, fKnob), alpha);
    p.circle(fState.fBounds.fLeft + fKnobInset +
                 (fState.fBounds.width() - fKnobInset * 2.0f) * fKnob,
             fState.fBounds.centerY(), fKnobRadius, fTheme.fText, alpha);
    if (this->focused()) {
      p.strokeRounded(fState.fBounds, fState.fBounds.height() * 0.5f, fTheme.fAccent, 1.5f,
                      alpha);
    }
  }

  [[nodiscard]] bool acceptsInput() const { return skiff::scene::acts(fOnToggle); }
  [[nodiscard]] bool focusChangesAppearance() const {
    return skiff::scene::acts(fOnToggle);
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::toggle{};
    out.fValue = fOn ? "on" : "off";
    out.fActions = {skiff::scene::semantic_action::focus{},
                    skiff::scene::semantic_action::activate{}};
    return out;
  }

  [[nodiscard]] bool onClick(float, float) {
    if (!skiff::scene::acts(fOnToggle)) {
      return false;
    }
    std::invoke(fOnToggle);
    return true;
  }

private:
  [[nodiscard]] static skia::SkColor mix(skia::SkColor a, skia::SkColor b,
                                         float t) {
    const auto channel = [t](std::uint32_t from, std::uint32_t to) {
      return static_cast<std::uint8_t>(
          static_cast<float>(from) +
          (static_cast<float>(to) - static_cast<float>(from)) * t);
    };
    return skia::colorSetARGB(channel((a >> 24) & 0xffu, (b >> 24) & 0xffu),
                              channel((a >> 16) & 0xffu, (b >> 16) & 0xffu),
                              channel((a >> 8) & 0xffu, (b >> 8) & 0xffu),
                              channel(a & 0xffu, b & 0xffu));
  }

  [[no_unique_address]] OnToggle fOnToggle;
  bool fOn = false;
  float fKnob = 0.0f;
  double fLastMs = 0.0;
};
Toggle() -> Toggle<>;

} // namespace skiff::widgets
