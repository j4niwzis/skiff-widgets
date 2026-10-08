export module skiff.widgets.button;

import std;
import skia;
import skiff.paint;
import skiff.scene;
export import skiff.widgets.theme;
export import skiff.widgets.erased;

namespace skiff::widgets {
using skiff::scene::Margin;
using skiff::scene::Spec;
} // namespace skiff::widgets

export namespace skiff::widgets {

namespace internal {
// A rounded rectangle with a label in it that calls something when clicked,
// and lightens under the pointer. AdwButton, and the five hand-written ones
// this replaces.
template <class Action = skiff::scene::NoAction>
class Button : public skiff::scene::Node {
public:
  explicit Button(std::string label, Action action = {})
      : fLabel(std::move(label)), fAction(std::move(action)) {
    fState.fHeight = fTheme.fRowHeight;
  }
  // Made in a theme: the one it is given, not one of the library's.
  Button(Theme theme, std::string label, Action action = {})
      : Button(std::move(label), std::move(action)) {
    fTheme = std::move(theme);
    fState.fHeight = fTheme.fRowHeight;
  }

  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }

  void setPrimary(bool primary) {
    if (primary == fPrimary) {
      return;
    }
    fPrimary = primary;
    this->markDamaged();
  }
  [[nodiscard]] bool primary() const noexcept { return fPrimary; }

  void setOutlined(bool outlined) {
    if (outlined == fOutlined) {
      return;
    }
    fOutlined = outlined;
    this->markDamaged();
  }
  [[nodiscard]] bool outlined() const noexcept { return fOutlined; }

  void setAccent(skia::SkColor accent) {
    if (accent == fTheme.fAccent) {
      return;
    }
    fTheme.fAccent = accent;
    this->markDamaged();
  }

  void setEnabled(bool enabled) {
    if (enabled == fEnabled) {
      return;
    }
    fEnabled = enabled;
    this->setDisabled(!enabled);
  }
  [[nodiscard]] bool enabled() const noexcept { return fEnabled; }

  void setLabel(std::string label) {
    if (label == fLabel) {
      return;
    }
    fLabel = std::move(label);
    this->markDamaged();
  }

public:
  Theme fTheme;
  [[nodiscard]] bool acceptsInput() const { return fEnabled; }
  [[nodiscard]] bool hoverChangesAppearance() const { return fEnabled; }
  [[nodiscard]] bool focusChangesAppearance() const { return fEnabled; }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::button{};
    out.fLabel = fLabel;
    out.fDisabled = !fEnabled;
    out.fActions = {skiff::scene::semantic_action::focus{},
                    skiff::scene::semantic_action::activate{}};
    return out;
  }

  [[nodiscard]] bool onClick(float x, float y) {
    if (!fEnabled || !fState.fBounds.contains(x, y)) {
      return false;
    }
    this->act();
    return true;
  }
  // What a press asks for, where its action answers: asked as the press is
  // delivered.
  auto onPress()
    requires skiff::scene::Answering<Action>
  {
    return std::invoke(fAction);
  }

private:
  void act()
    requires skiff::scene::Answering<Action>
  {
    skiff::scene::pressLater(fState);
  }
  void act() { std::invoke(fAction); }

public:

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    const bool hot = (fState.fHovered || this->showsFocus()) && fEnabled;
    skia::SkColor fill = fPrimary ? fTheme.fAccent : fTheme.fSurface;
    if (hot) {
      fill =
          fPrimary ? skiff::paint::lighten(fill, 0.12f) : fTheme.fSurfaceHover;
    }
    p.fillRounded(fState.fBounds, fTheme.fCorner, fill,
                  alpha * (fEnabled ? 1.0f : 0.5f));
    if (fOutlined && !fPrimary) {
      p.strokeRounded(fState.fBounds, fTheme.fCorner, fTheme.fAccent,
                      hot ? 3.0f : 2.0f,
                      alpha * (fEnabled ? 1.0f : 0.5f));
    }
    const skia::SkColor text =
        fPrimary ? fTheme.fOnAccent
                 : (fOutlined && hot ? fTheme.fAccent : fTheme.fText);
    p.textCentredIn(fState.fBounds, fLabel, fTheme.fFontSize,
                    text,
                    alpha * (fEnabled ? 1.0f : 0.5f), true);
  }

  // The action it calls, for what holds the button to read it back.
  Action &action() noexcept { return fAction; }
  // Where its action keeps the events it sent (fEmitted): taken by a walk
  // that drains them, as a model's widget's.
  auto takeEvents()
    requires requires(Action &a) { a.fEmitted; }
  {
    return std::exchange(fAction.fEmitted, {});
  }

private:
  bool fPrimary = false; // filled in the accent rather than the surface
  bool fOutlined = false;
  bool fEnabled = true;
  std::string fLabel;
  [[no_unique_address]] Action fAction;
};

} // namespace internal

// The button over AnyAction, taking an action of its own type and erasing
// it: all of its code but this is internal::Button<AnyAction>'s, made once.
template <class Action>
class ErasedButton : public internal::Button<AnyActionFor<Action>> {
  using Base = internal::Button<AnyActionFor<Action>>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  explicit ErasedButton(std::string label, Action action = {})
      : internal::Button<AnyActionFor<Action>>(std::move(label), AnyActionFor<Action>(std::move(action))) {}
  ErasedButton(Theme theme, std::string label, Action action = {})
      : internal::Button<AnyActionFor<Action>>(std::move(theme), std::move(label), AnyActionFor<Action>(std::move(action))) {}
};

// The button: made for its action in a release build, over AnyAction
// otherwise.
template <class Action = skiff::scene::NoAction>
using Button = std::conditional_t<kErasedActions && !KeepsEvents<Action>, ErasedButton<Action>, internal::Button<Action>>;

} // namespace skiff::widgets
