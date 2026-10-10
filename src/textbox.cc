export module skiff.widgets.textbox;

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

// A single line of editable text: OsuTextBox, AdwEntryRow. It owns the string
// and reports changes. Text and composition events arrive through the scene's
// focus router, so screens do not need a parallel keyboard implementation.
namespace internal {
template <class OnChanged = skiff::scene::NoAction>
class TextBox : public skiff::scene::Node {
public:
  explicit TextBox(std::string placeholder = {}, OnChanged onChanged = {})
      : fPlaceholder(std::move(placeholder)), fOnChanged(std::move(onChanged)) {
    fState.fRelativeSizeAxes = skiff::scene::axes::kX;
    fState.fWidth = 1.0f;
    fState.fHeight = fTheme.fRowHeight;
    fState.setCursor(skiff::scene::cursor::text{});
  }
  // Made in a theme: the one it is given, not one of the library's.
  TextBox(Theme theme, std::string placeholder, OnChanged onChanged = {})
      : TextBox(std::move(placeholder), std::move(onChanged)) {
    fTheme = std::move(theme);
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
    fAtoms.clear();
    fCaret = fText.size();
    fAll = false;
    this->markDamaged();
  }
  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  // Copied inline images keep their source while this single-line field
  // displays their plain label. Offsets are in text(), not placeholders.
  [[nodiscard]] const std::vector<skiff::scene::ClipboardAtom>& atoms() const noexcept { return fAtoms; }

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
  Theme fTheme;
  std::string fPlaceholder;
  bool fSearchIcon = false; // the magnifier lazer puts in its search boxes
  bool fMasked = false;
  // Space owned by a trailing status, clear button or other overlay.
  float fTrailingInset = 0.0f;

  [[nodiscard]] bool acceptsInput() const { return true; }
  // A text field is what a press gives the focus to.
  [[nodiscard]] bool takesFocusOnPress() const { return true; }
  [[nodiscard]] bool focusChangesAppearance() const { return true; }
  // The caret blinks by itself while the box has the focus, and goes with
  // it: frames only while that is so, one a flip.
  [[nodiscard]] bool settling() const { return this->focused() || fCaretShown; }
  // Ticked while it has the caret.
  [[nodiscard]] bool wantsTick() const { return this->focused(); }
  void update(double nowMs) { this->tickCaret(nowMs, this->focused()); }

  [[nodiscard]] bool onClick(float x, float y) {
    return fState.fBounds.contains(x, y);
  }
  // A right press -- a long press, on a phone: the text menu asked of the
  // host. Any other press is the click's.
  using Node::onPointer;
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::down &at,
                 skiff::scene::PointerReply &reply) {
    if (at.button != 3) {
      return;
    }
    skiff::scene::textMenusAsked().push_back(skiff::scene::text_menu::of_field{.selection = fAll, .masked = fMasked, .text = !fText.empty()});
    reply.handle();
  }

  using Node::onText;
  void onText(skiff::scene::phase::target, const skiff::scene::text::commit &typed,
              skiff::scene::Reply &reply) {
    if (!typed.text.empty()) {
      this->takeAll();
      this->replaceText(fCaret, fCaret, typed.text);
      fCaret += typed.text.size();
      this->changed();
    }
    fComposition.clear();
    this->markDamaged();
    reply.handle();
  }
  void onText(skiff::scene::phase::target,
              const skiff::scene::text::compose &composing,
              skiff::scene::Reply &reply) {
    fComposition = std::string(composing.text);
    fCompositionSelectionStart = composing.start;
    fCompositionSelectionLength = composing.length;
    this->markDamaged();
    reply.handle();
  }

  using Node::onKey;
  void onKey(skiff::scene::phase::target, const skiff::scene::key::down &press,
             skiff::scene::Reply &reply) {
    namespace keys = skiff::scene::keys;
    const bool control = press.modifiers.has<skiff::scene::modifier::control>();
    // The clipboard, as in every field: Ctrl+V pastes at the caret (over
    // all of it where all is selected), newlines as spaces in a line;
    // Ctrl+A selects all of it, and Ctrl+C and Ctrl+X copy and cut that.
    // Nothing selected, Ctrl+C is not this field's: the window may copy
    // what is selected elsewhere.
    if (control && press.key == keys::kV) {
      const std::string copied = skiff::scene::clipboardText();
      const auto rich = skiff::scene::clipboardRichText();
      const std::string pasted = singleLine(copied);
      this->takeAll();
      this->replaceText(fCaret, fCaret, pasted);
      if (rich && rich->text == copied && !press.modifiers.has<skiff::scene::modifier::shift>()) {
        std::ptrdiff_t shift = 0;
        for (const auto& atom : rich->atoms) {
          if (atom.first > atom.last || atom.last > rich->display.size()) continue;
          const auto first = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(atom.first) + shift);
          const auto label = singleLine(atom.plain);
          const auto offset = singleLine(std::string_view(copied).substr(0, first)).size();
          fAtoms.push_back({fCaret + offset, fCaret + offset + label.size(), atom.target, label, atom.picture});
          shift += static_cast<std::ptrdiff_t>(atom.plain.size()) - static_cast<std::ptrdiff_t>(atom.last - atom.first);
        }
        std::ranges::sort(fAtoms, {}, &skiff::scene::ClipboardAtom::first);
      }
      fCaret += pasted.size();
      this->changed();
      this->markDamaged();
      reply.handle();
      return;
    }
    if (control && press.key == keys::kA) {
      fAll = !fText.empty();
      this->markDamaged();
      reply.handle();
      return;
    }
    if (control && (press.key == keys::kC || press.key == keys::kX)) {
      if (!fAll || fMasked) {
        return;
      }
      skiff::scene::clipboardCandidate() = skiff::scene::clipboardFragment(fText, fAtoms);
      skiff::scene::setClipboardText(fText);
      if (press.key == keys::kX) {
        this->takeAll();
        this->changed();
      }
      this->markDamaged();
      reply.handle();
      return;
    }
    // All selected: a Backspace or Delete takes all of it; a move lets the
    // selection go.
    if (fAll && (press.key == keys::kBackspace || press.key == keys::kDelete)) {
      this->takeAll();
      this->changed();
      this->markDamaged();
      reply.handle();
      return;
    }
    if (fAll) {
      fAll = false;
      this->markDamaged();
    }
    if (press.key == keys::kBackspace) {
      if (fCaret > 0) {
        const std::size_t eraseFrom = previousCodepoint(fText, fCaret);
        this->replaceText(eraseFrom, fCaret, {});
        fCaret = eraseFrom;
        this->changed();
        this->markDamaged();
      }
      reply.handle();
    } else if (press.key == keys::kDelete) {
      if (fCaret < fText.size()) {
        const std::size_t eraseTo = nextCodepoint(fText, fCaret);
        this->replaceText(fCaret, eraseTo, {});
        this->changed();
        this->markDamaged();
      }
      reply.handle();
    } else if (press.key == keys::kLeft) {
      fCaret = previousCodepoint(fText, fCaret);
      this->markDamaged();
      reply.handle();
    } else if (press.key == keys::kRight) {
      fCaret = nextCodepoint(fText, fCaret);
      this->markDamaged();
      reply.handle();
    } else if (press.key == keys::kHome) {
      fCaret = 0;
      this->markDamaged();
      reply.handle();
    } else if (press.key == keys::kEnd) {
      fCaret = fText.size();
      this->markDamaged();
      reply.handle();
    } else if (press.key == keys::kEscape && !fComposition.empty()) {
      fComposition.clear();
      this->markDamaged();
      reply.handle();
    }
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::text_box{};
    out.fLabel = fPlaceholder;
    if (!fMasked) {
      out.fValue = fText;
    }
    out.fActions = {skiff::scene::semantic_action::focus{},
                    skiff::scene::semantic_action::set_value{}};
    return out;
  }

  using Node::onSemantic;
  void onSemantic(skiff::scene::phase::target,
                  const skiff::scene::semantic_action::set_value &set,
                  skiff::scene::Reply &reply) {
    if (set.text != fText) {
      this->setText(std::string(set.text));
      this->changed();
    }
    reply.handle();
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    // As Telegram's and Element's fields: the plate the same focused or
    // not, a thin accent border and the caret saying it has the focus --
    // never the text on an accent fill.
    const bool active = this->selected() || this->focused();
    p.fillRounded(fState.fBounds, fTheme.fCorner, fTheme.fSurface, alpha);
    if (active) {
      skia::SkPaint border;
      border.setAntiAlias(true);
      border.setStyle(skia::kStrokeStyle);
      border.setStrokeWidth(1.5f);
      border.setColor(fTheme.fAccent);
      border.setAlphaf(alpha);
      canvas->drawRoundRect(fState.fBounds.makeInset(0.75f, 0.75f), fTheme.fCorner, fTheme.fCorner, border);
    }
    const skia::SkColor textColour = fTheme.fText;

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
    // All of it selected: on a plate of the accent, as a selection is.
    if (fAll && !shown.empty()) {
      const float wide = std::min(room, p.measure(shown, fTheme.fFontSize));
      p.fillRounded(skia::SkRect::MakeXYWH(textLeft - 1.0f, fState.fBounds.centerY() - fTheme.fFontSize * 0.75f, wide + 2.0f,
                                           fTheme.fFontSize * 1.5f),
                    2.0f, fTheme.fAccent, alpha * 0.35f);
    }
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

  // What it calls when its text changes, for what holds it to read back.
  OnChanged &onChanged() noexcept { return fOnChanged; }

private:
  static std::string singleLine(std::string_view text) {
    std::string out(text);
    std::ranges::replace(out, '\n', ' ');
    std::erase(out, '\r');
    return out;
  }
  void replaceText(std::size_t first, std::size_t last, std::string_view text) {
    // Editing inside a label turns it into ordinary text. Edits beside it
    // retain its identity and shift its offsets with the rest of the line.
    std::erase_if(fAtoms, [&](const auto& atom) { return atom.first < last && atom.last > first; });
    const auto shift = static_cast<std::ptrdiff_t>(text.size()) - static_cast<std::ptrdiff_t>(last - first);
    for (auto& atom : fAtoms)
      if (atom.first >= last) {
        atom.first = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(atom.first) + shift);
        atom.last = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(atom.last) + shift);
      }
    fText.replace(first, last - first, text);
  }
  // All of it selected: taken out, the caret at the start.
  void takeAll() {
    if (!fAll) {
      return;
    }
    fAll = false;
    fText.clear();
    fAtoms.clear();
    fCaret = 0;
  }
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

  // What it was changed to, told: where the action answers, as the press
  // is delivered (onPress); else at once.
  void changed()
    requires skiff::scene::Answering<OnChanged>
  {
    skiff::scene::pressLater(fState);
  }
  void changed() { std::invoke(fOnChanged, std::string_view(fText)); }

public:
  auto onPress()
    requires skiff::scene::Answering<OnChanged>
  {
    return std::invoke(fOnChanged, std::string_view(fText));
  }

private:
  [[no_unique_address]] OnChanged fOnChanged;

public:

private:
  std::string fText;
  std::vector<skiff::scene::ClipboardAtom> fAtoms;
  std::string fComposition;
  std::size_t fCaret = 0;
  bool fAll = false;  // all of the text selected, by Ctrl+A
  int fCompositionSelectionStart = 0;
  int fCompositionSelectionLength = 0;
  bool fCaretShown = false;
};
} // namespace internal

// The box over an erased call, taking one of its own type: all of its code
// but this is internal::TextBox<AnyCall<void(std::string_view)>>'s, made once.
template <class OnChanged>
class ErasedTextBox : public internal::TextBox<AnyCallFor<void(std::string_view), OnChanged>> {
  using Base = internal::TextBox<AnyCallFor<void(std::string_view), OnChanged>>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  explicit ErasedTextBox(std::string placeholder = {}, OnChanged onChanged = {})
      : Base(std::move(placeholder), AnyCallFor<void(std::string_view), OnChanged>(std::move(onChanged))) {}
  ErasedTextBox(Theme theme, std::string placeholder, OnChanged onChanged = {})
      : Base(std::move(theme), std::move(placeholder), AnyCallFor<void(std::string_view), OnChanged>(std::move(onChanged))) {}
};
// The box: made for its call in a release build, over an erased one
// otherwise.
template <class OnChanged = skiff::scene::NoAction>
using TextBox = std::conditional_t<kErasedActions && !AnswersPresses<OnChanged>, ErasedTextBox<OnChanged>, internal::TextBox<OnChanged>>;


} // namespace skiff::widgets
