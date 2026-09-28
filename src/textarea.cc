export module skiff.widgets.textarea;

import std;
import skia;
import skiff.paint;
import skiff.scene;
export import skiff.widgets.theme;

export namespace skiff::widgets {

// Several lines of editable text, as a chat's message field: it wraps at its
// width, grows with its text up to a number of lines and then scrolls, and
// has a thin blinking caret and no fill of its own -- what holds it draws
// the field. Enter submits (`onSubmit(text)`), Shift+Enter starts a new
// line.
template <class OnSubmit = skiff::scene::NoAction>
class TextArea : public skiff::scene::Node {
public:
  explicit TextArea(std::string placeholder = {}, OnSubmit onSubmit = {})
      : fPlaceholder(std::move(placeholder)), fOnSubmit(std::move(onSubmit)) {
    fState.fHeight = this->heightFor(1);
  }

  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  void setText(std::string text) {
    fText = std::move(text);
    fCaret = fText.size();
    this->edited();
  }
  void setMaxLines(int lines) {
    fMaxLines = std::max(1, lines);
    this->invalidateLayout();
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }
  void setFontSize(float size) {
    fFontSize = size;
    this->invalidateLayout();
  }

  // As tall as its lines, up to the most it shows.
  void measure(const skia::SkRect &parent) {
    const float width = fState.fRelativeSizeAxes.has<skiff::scene::axis::x>()
                            ? parent.width() * fState.fWidth
                            : fState.fWidth;
    this->wrap(width);
    fState.fHeight = this->heightFor(
        std::clamp(static_cast<int>(fLines.size()), 1, fMaxLines));
  }

  [[nodiscard]] bool acceptsInput() const { return true; }
  [[nodiscard]] bool onClick(float x, float y) {
    fCaret = this->offsetAt(x, y);
    this->showCaret();
    return true;
  }
  [[nodiscard]] bool settling() const { return this->focused(); }
  void update(double nowMs) {
    const bool shown =
        this->focused() && std::fmod(nowMs - fCaretSinceMs, 1060.0) < 530.0;
    fNowMs = nowMs;
    if (shown != fCaretShown) {
      fCaretShown = shown;
      this->markDamaged();
    }
  }

  using Node::onText;
  void onText(skiff::scene::phase::target, const skiff::scene::text::commit &typed,
              skiff::scene::Reply &reply) {
    if (!typed.text.empty()) {
      fText.insert(fCaret, typed.text);
      fCaret += typed.text.size();
      this->edited();
    }
    reply.handle();
  }

  using Node::onKey;
  void onKey(skiff::scene::phase::target, const skiff::scene::key::down &press,
             skiff::scene::Reply &reply) {
    namespace keys = skiff::scene::keys;
    namespace modifier = skiff::scene::modifier;
    if (press.key == keys::kEnter) {
      if (press.modifiers.has<modifier::shift>()) {
        fText.insert(fCaret, "\n");
        ++fCaret;
        this->edited();
      } else if (skiff::scene::acts(fOnSubmit)) {
        std::invoke(fOnSubmit, std::string_view(fText));
      }
    } else if (press.key == keys::kBackspace) {
      if (fCaret == 0) {
        return reply.handle();
      }
      const std::size_t from = previous(fText, fCaret);
      fText.erase(from, fCaret - from);
      fCaret = from;
      this->edited();
    } else if (press.key == keys::kDelete) {
      if (fCaret < fText.size()) {
        fText.erase(fCaret, next(fText, fCaret) - fCaret);
        this->edited();
      }
    } else if (press.key == keys::kLeft) {
      fCaret = previous(fText, fCaret);
    } else if (press.key == keys::kRight) {
      fCaret = next(fText, fCaret);
    } else if (press.key == keys::kHome) {
      fCaret = fLines.empty() ? 0 : fLines[this->lineOf(fCaret)].fStart;
    } else if (press.key == keys::kEnd) {
      fCaret = fLines.empty() ? fText.size() : fLines[this->lineOf(fCaret)].fEnd;
    } else if (press.key == keys::kUp || press.key == keys::kDown) {
      this->moveLine(press.key == keys::kUp ? -1 : 1);
    } else {
      return;
    }
    this->showCaret();
    reply.handle();
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::text_box{};
    out.fLabel = fPlaceholder;
    out.fValue = fText;
    out.fActions = {skiff::scene::semantic_action::focus{}};
    return out;
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    const skia::SkRect &box = fState.fBounds;
    const int save = canvas->save();
    canvas->clipRect(box, true);
    const float lineHeight = fFontSize * kLineSpacing;
    if (fText.empty()) {
      p.text(fPlaceholder, box.fLeft, box.fTop + kPadY + fFontSize, fFontSize,
             fTheme.fTextFaint, alpha);
    }
    const int first = this->firstShown();
    float y = box.fTop + kPadY + fFontSize;
    for (int i = first; i < static_cast<int>(fLines.size()); ++i) {
      const Line &line = fLines[static_cast<std::size_t>(i)];
      p.text(fText.substr(line.fStart, line.fEnd - line.fStart), box.fLeft, y,
             fFontSize, fTheme.fText, alpha);
      y += lineHeight;
    }
    if (fCaretShown) {
      const std::size_t at = fLines.empty() ? 0 : this->lineOf(fCaret);
      const float x =
          fLines.empty()
              ? 0.0f
              : p.measure(fText.substr(fLines[at].fStart, fCaret - fLines[at].fStart),
                          fFontSize);
      const float top = box.fTop + kPadY +
                        static_cast<float>(static_cast<int>(at) - first) * lineHeight;
      p.fillRect(skia::SkRect::MakeXYWH(box.fLeft + x, top + 1.0f, 1.2f, fFontSize + 3.0f),
                 fTheme.fText, alpha);
    }
    canvas->restoreToCount(save);
  }

private:
  static constexpr float kPadY = 6.0f;
  static constexpr float kLineSpacing = 1.3f;

  // A line as drawn: the bytes of the text it shows. A line ends at a
  // newline (which it does not show) or where the next word no longer fits.
  struct Line {
    std::size_t fStart = 0;
    std::size_t fEnd = 0;
  };

  [[nodiscard]] float heightFor(int lines) const {
    return static_cast<float>(lines) * fFontSize * kLineSpacing + 2.0f * kPadY;
  }

  void edited() {
    this->showCaret();
    this->invalidateLayout();
    this->markDamaged();
  }
  void showCaret() {
    fCaretSinceMs = fNowMs;
    fCaretShown = this->focused();
    this->markDamaged();
  }

  // The lines of the text at `width`, greedily by words; a word wider than
  // the line is broken where it must be.
  void wrap(float width) {
    fLines.clear();
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr || width <= 0.0f) {
      fLines.push_back({0, fText.size()});
      return;
    }
    const skiff::paint::Painter p(nullptr, *font);
    const auto fits = [&](std::size_t from, std::size_t to) {
      return p.measure(fText.substr(from, to - from), fFontSize) <= width;
    };
    std::size_t start = 0;
    while (true) {
      const std::size_t newline = fText.find('\n', start);
      const std::size_t stop = newline == std::string::npos ? fText.size() : newline;
      std::size_t from = start;
      while (!fits(from, stop)) {
        // The last space that still fits, or as many codepoints as fit.
        std::size_t cut = from;
        for (std::size_t at = from; at < stop; at = next(fText, at)) {
          if (fText[at] == ' ' && at > from && fits(from, at)) {
            cut = at;
          }
        }
        if (cut == from) {
          cut = next(fText, from);
          while (cut < stop && fits(from, next(fText, cut))) {
            cut = next(fText, cut);
          }
          fLines.push_back({from, cut});
          from = cut;
        } else {
          fLines.push_back({from, cut});
          from = cut + 1; // the space the line broke at
        }
      }
      fLines.push_back({from, stop});
      if (newline == std::string::npos) {
        break;
      }
      start = newline + 1;
    }
  }

  // The line the caret at `offset` is on: the last that starts at or before
  // it.
  [[nodiscard]] std::size_t lineOf(std::size_t offset) const {
    std::size_t at = 0;
    for (std::size_t i = 0; i < fLines.size(); ++i) {
      if (fLines[i].fStart <= offset) {
        at = i;
      }
    }
    return at;
  }
  // The first line shown: the caret's line is always in view.
  [[nodiscard]] int firstShown() const {
    if (fLines.empty()) {
      return 0;
    }
    const int caretLine = static_cast<int>(this->lineOf(fCaret));
    return std::max(0, caretLine - fMaxLines + 1);
  }
  // The offset nearest to a point.
  [[nodiscard]] std::size_t offsetAt(float x, float y) const {
    if (fLines.empty()) {
      return fText.size();
    }
    const float lineHeight = fFontSize * kLineSpacing;
    const int row = this->firstShown() +
                    static_cast<int>((y - fState.fBounds.fTop - kPadY) / lineHeight);
    const Line &line =
        fLines[static_cast<std::size_t>(std::clamp(row, 0, static_cast<int>(fLines.size()) - 1))];
    return this->offsetIn(line, x - fState.fBounds.fLeft);
  }
  [[nodiscard]] std::size_t offsetIn(const Line &line, float x) const {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return line.fEnd;
    }
    const skiff::paint::Painter p(nullptr, *font);
    std::size_t best = line.fStart;
    for (std::size_t at = line.fStart; at <= line.fEnd; at = next(fText, at)) {
      if (p.measure(fText.substr(line.fStart, at - line.fStart), fFontSize) <= x) {
        best = at;
      }
      if (at == line.fEnd) {
        break;
      }
    }
    return best;
  }
  void moveLine(int by) {
    if (fLines.empty()) {
      return;
    }
    const std::size_t at = this->lineOf(fCaret);
    const int to = static_cast<int>(at) + by;
    if (to < 0 || to >= static_cast<int>(fLines.size())) {
      fCaret = by < 0 ? 0 : fText.size();
      return;
    }
    skia::SkFont *font = skiff::paint::defaultFont();
    const float x =
        font == nullptr
            ? 0.0f
            : skiff::paint::Painter(nullptr, *font)
                  .measure(fText.substr(fLines[at].fStart, fCaret - fLines[at].fStart),
                           fFontSize);
    fCaret = this->offsetIn(fLines[static_cast<std::size_t>(to)], x);
  }

  [[nodiscard]] static std::size_t previous(std::string_view text, std::size_t at) {
    if (at == 0) {
      return 0;
    }
    --at;
    while (at > 0 && (static_cast<unsigned char>(text[at]) & 0xC0u) == 0x80u) {
      --at;
    }
    return at;
  }
  [[nodiscard]] static std::size_t next(std::string_view text, std::size_t at) {
    if (at >= text.size()) {
      return text.size();
    }
    ++at;
    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0u) == 0x80u) {
      ++at;
    }
    return at;
  }

  std::string fText;
  std::string fPlaceholder;
  [[no_unique_address]] OnSubmit fOnSubmit;
  Theme fTheme = theme();
  float fFontSize = 15.0f;
  int fMaxLines = 8;
  std::vector<Line> fLines;
  std::size_t fCaret = 0;
  bool fCaretShown = false;
  double fCaretSinceMs = 0.0;
  double fNowMs = 0.0;
};

} // namespace skiff::widgets
