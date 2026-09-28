export module skiff.widgets.textbox;

import std;
import skia;
import skiff.paint;
import skiff.scene;
export import skiff.widgets.theme;

namespace skiff::widgets {
using skiff::scene::Anchor;
using skiff::scene::Axes;
using skiff::scene::Easing;
using skiff::scene::Margin;
using skiff::scene::Spec;
} // namespace skiff::widgets

export namespace skiff::widgets {

// A single line of editable text: OsuTextBox, AdwEntryRow. It owns the string
// and reports changes. Text and composition events arrive through the scene's
// focus router, so screens do not need a parallel keyboard implementation.
template <class OnChanged = skiff::scene::NoAction>
class TextBox : public skiff::scene::Node {
public:
  explicit TextBox(std::string placeholder = {}, OnChanged onChanged = {})
      : fPlaceholder(std::move(placeholder)), fOnChanged(std::move(onChanged)) {
    fState.fRelativeSizeAxes = Axes::kX;
    fState.fWidth = 1.0f;
    fState.fHeight = fTheme.fRowHeight;
  }

  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }
  void setSearchIcon(bool enabled) {
    if (enabled != fSearchIcon) {
      fSearchIcon = enabled;
      this->markDamaged();
    }
  }
  // A password: each character drawn as a dot, and the value kept from
  // assistive technology, which would otherwise read it aloud.
  void setMasked(bool masked) {
    if (masked != fMasked) {
      fMasked = masked;
      this->markDamaged();
    }
  }
  [[nodiscard]] bool masked() const noexcept { return fMasked; }
  void setTrailingInset(float inset) {
    if (inset != fTrailingInset) {
      fTrailingInset = inset;
      this->markDamaged();
    }
  }

  void setText(std::string text) {
    if (text == fText) {
      return;
    }
    fText = std::move(text);
    fCaret = fText.size();
    this->markDamaged();
  }
  [[nodiscard]] const std::string &text() const noexcept { return fText; }

  // The caret is usually the only thing on a screen that changes without
  // being touched, so it marks the box when it flips and nothing else: the
  // frame after a flip repaints one rectangle, and the frames between are
  // not drawn at all. Off screen it does not blink, because a caret nobody
  // can see is not worth a frame.
  void tickCaret(double nowMs, bool visible) {
    const bool shown = visible && std::fmod(nowMs, 1000.0) < 600.0;
    if (shown != fCaretShown) {
      fCaretShown = shown;
      this->markDamaged();
    }
  }

public:
  Theme fTheme = theme();
  std::string fPlaceholder;
  bool fSearchIcon = false; // the magnifier lazer puts in its search boxes
  bool fMasked = false;
  // Space owned by a trailing status, clear button or other overlay.
  float fTrailingInset = 0.0f;

  [[nodiscard]] bool acceptsInput() const { return true; }
  [[nodiscard]] bool focusChangesAppearance() const { return true; }

  [[nodiscard]] bool onClick(float x, float y) {
    return fState.fBounds.contains(x, y);
  }

  void onTextInput(skiff::scene::TextInputEvent &event) {
    if (event.fPhase != skiff::scene::EventPhase::kTarget) {
      return;
    }
    if (event.fCommit) {
      if (!event.fText.empty()) {
        fText.insert(fCaret, event.fText);
        fCaret += event.fText.size();
        std::invoke(fOnChanged, std::string_view(fText));
      }
      fComposition.clear();
    } else {
      fComposition = event.fComposition.empty() ? std::string(event.fText)
                                                : std::string(event.fComposition);
      fCompositionSelectionStart = event.fSelectionStart;
      fCompositionSelectionLength = event.fSelectionLength;
    }
    this->markDamaged();
    event.handle();
  }

  void onKeyEvent(skiff::scene::KeyEvent &event) {
    if (event.fPhase != skiff::scene::EventPhase::kTarget ||
        !event.fPressed) {
      return;
    }
    if (event.fKey == skiff::scene::Key::kBackspace) {
      if (fCaret > 0) {
        const std::size_t eraseFrom = previousCodepoint(fText, fCaret);
        fText.erase(eraseFrom, fCaret - eraseFrom);
        fCaret = eraseFrom;
        std::invoke(fOnChanged, std::string_view(fText));
        this->markDamaged();
      }
      event.handle();
    } else if (event.fKey == skiff::scene::Key::kDelete) {
      if (fCaret < fText.size()) {
        const std::size_t eraseTo = nextCodepoint(fText, fCaret);
        fText.erase(fCaret, eraseTo - fCaret);
        std::invoke(fOnChanged, std::string_view(fText));
        this->markDamaged();
      }
      event.handle();
    } else if (event.fKey == skiff::scene::Key::kLeft) {
      fCaret = previousCodepoint(fText, fCaret);
      this->markDamaged();
      event.handle();
    } else if (event.fKey == skiff::scene::Key::kRight) {
      fCaret = nextCodepoint(fText, fCaret);
      this->markDamaged();
      event.handle();
    } else if (event.fKey == skiff::scene::Key::kHome) {
      fCaret = 0;
      this->markDamaged();
      event.handle();
    } else if (event.fKey == skiff::scene::Key::kEnd) {
      fCaret = fText.size();
      this->markDamaged();
      event.handle();
    } else if (event.fKey == skiff::scene::Key::kEscape &&
               !fComposition.empty()) {
      fComposition.clear();
      this->markDamaged();
      event.handle();
    }
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::SemanticRole::kTextBox;
    out.fLabel = fPlaceholder;
    if (!fMasked) {
      out.fValue = fText;
    }
    out.fActions = {skiff::scene::SemanticAction::kFocus,
                    skiff::scene::SemanticAction::kSetValue};
    return out;
  }

  void onSemanticAction(skiff::scene::SemanticActionEvent &event) {
    if (event.fAction == skiff::scene::SemanticAction::kSetValue) {
      if (event.fText != fText) {
        this->setText(std::string(event.fText));
        std::invoke(fOnChanged, std::string_view(fText));
      }
      event.handle();
    } else {
      skiff::scene::defaultSemanticAction(*this, event);
    }
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    const bool active = this->selected() || this->focused();
    p.fillRounded(fState.fBounds, fTheme.fCorner,
                  active ? fTheme.fAccent : fTheme.fSurface, alpha);
    const skia::SkColor textColour =
        active ? fTheme.fOnAccent : fTheme.fText;

    float textLeft = fState.fBounds.fLeft + fTheme.fPaddingX;
    if (fSearchIcon) {
      skia::SkPaint icon;
      icon.setAntiAlias(true);
      icon.setStyle(skia::kStrokeStyle);
      icon.setStrokeWidth(1.8f);
      icon.setColor(fTheme.fTextDim);
      icon.setAlphaf(alpha);
      const float ix = fState.fBounds.fLeft + fTheme.fPaddingX + 6.0f;
      const float iy = fState.fBounds.centerY();
      canvas->drawCircle(ix, iy - 1.0f, 5.5f, icon);
      canvas->drawLine(ix + 4.0f, iy + 3.0f, ix + 8.0f, iy + 7.0f, icon);
      textLeft = ix + 14.0f;
    }

    const float baseline = p.middleBaseline(fState.fBounds, fTheme.fFontSize);
    const float room = std::max(
        0.0f, fState.fBounds.fRight - textLeft - fTheme.fPaddingX - fTrailingInset);
    const std::string beforeCaret = shownAs(fText.substr(0, fCaret) + fComposition);
    const std::string shown = beforeCaret + shownAs(fText.substr(fCaret));
    if (shown.empty()) {
      p.text(fPlaceholder, textLeft, baseline, fTheme.fFontSize,
             fTheme.fTextFaint, alpha * 0.6f);
    } else {
      p.textClipped(shown, textLeft, baseline, room, fTheme.fFontSize,
                    textColour, alpha);
    }
    if (fCaretShown) {
      const float cx =
          textLeft +
          std::min(room,
                   p.measure(beforeCaret, fTheme.fFontSize)) +
          2.0f;
      p.fillRect(skia::SkRect::MakeXYWH(cx, fState.fBounds.centerY() - 9.0f, 1.5f,
                                        fTheme.fFontSize + 2.0f),
                 textColour, alpha * 0.8f);
    }
  }

private:
  // What is drawn for some of the text: itself, or a dot per character.
  [[nodiscard]] std::string shownAs(std::string_view text) const {
    if (!fMasked) {
      return std::string(text);
    }
    std::string out;
    for (std::size_t at = 0; at < text.size(); at = nextCodepoint(text, at)) {
      out += "\u2022";
    }
    return out;
  }

  [[nodiscard]] static std::size_t previousCodepoint(std::string_view text,
                                                      std::size_t from) {
    if (from == 0) {
      return 0;
    }
    --from;
    while (from > 0 &&
           (static_cast<unsigned char>(text[from]) & 0xc0u) == 0x80u) {
      --from;
    }
    return from;
  }

  [[nodiscard]] static std::size_t nextCodepoint(std::string_view text,
                                                  std::size_t from) {
    if (from >= text.size()) {
      return text.size();
    }
    ++from;
    while (from < text.size() &&
           (static_cast<unsigned char>(text[from]) & 0xc0u) == 0x80u) {
      ++from;
    }
    return from;
  }

  [[no_unique_address]] OnChanged fOnChanged;
  std::string fText;
  std::string fComposition;
  std::size_t fCaret = 0;
  int fCompositionSelectionStart = 0;
  int fCompositionSelectionLength = 0;
  bool fCaretShown = false;
};

TextBox() -> TextBox<>;
TextBox(const char *) -> TextBox<>;
TextBox(std::string) -> TextBox<>;

} // namespace skiff::widgets
