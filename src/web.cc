// skiff.widgets.web: the controls of skiff-widgets on skiff.web -- a model
// widget's same words (ToggleField, SliderField, TextField, ChoiceTabs,
// ChoiceList, SendButton; read() and onPress()) on the browser's own
// elements, dressed in the widgets' Theme by a stylesheet (themeCss). Each
// shows a part of the model and says what a press changes, as the Skia
// widgets do; what it is on the page is a native control, and what it was
// done to comes in through onDom.
export module skiff.widgets.web;

import std;
import skiff.core;
import skiff.model;
import skiff.bind;
export import skiff.web;
export import skiff.widgets.theme;

export namespace skiff::widgets::web {

using namespace skiff::web;
using skiff::web::Node;

// A colour brought towards white by a share, as skiff.paint lightens one.
[[nodiscard]] inline scene::Colour lighten(scene::Colour c, float amount) {
  const auto up = [&](unsigned v) { return static_cast<unsigned>(std::lround(v + (255.0f - v) * amount)); };
  return (c & 0xff000000u) | (up((c >> 16) & 0xffu) << 16) | (up((c >> 8) & 0xffu) << 8) | up(c & 0xffu);
}
[[nodiscard]] inline std::string themeCss(const Theme &t) {
  const std::string surface = rgba(t.fSurface), hover = rgba(t.fSurfaceHover), active = rgba(t.fSurfaceActive), text = rgba(t.fText),
                    label = rgba(t.fLabel), dim = rgba(t.fTextDim), faint = rgba(t.fTextFaint), accent = rgba(t.fAccent),
                    onAccent = rgba(t.fOnAccent), corner = px(t.fCorner), font = px(t.fFontSize), rowH = px(t.fRowHeight),
                    padX = px(t.fPaddingX), font600 = "600 " + px(t.fFontSize) + " system-ui, sans-serif";
  const std::string chevron =
      "url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6' viewBox='0 0 10 6'%3E%3Cpath d='M0 0l5 6 5-6z' fill='white'/%3E%3C/svg%3E\")";
  std::string css;
  css += ".skiff-text{color:" + label + ";font:" + font + " system-ui,sans-serif;line-height:1.3}.skiff-canvas{display:block}\n";
  css += ".skiff-button{height:" + rowH + ";padding:0 " + padX + ";border:0;border-radius:" + corner + ";background:" + surface + ";color:" + text +
         ";font:" + font600 + ";cursor:pointer}\n";
  css += ".skiff-button:hover{background:" + hover + "}\n";
  css += ".skiff-button:focus-visible{outline:1.5px solid " + accent + ";outline-offset:-1.5px}\n";
  css += ".skiff-button[disabled]{opacity:.5;cursor:default}\n";
  css += ".skiff-tabs{display:inline-flex;gap:14px;border-radius:3px}\n";
  css += ".skiff-tab{background:none;border:0;padding:4px 0;font:" + font + " system-ui,sans-serif;color:" + dim + ";cursor:pointer}\n";
  css += ".skiff-tab:hover{color:" + rgba(lighten(t.fTextDim, 0.2f)) + "}\n";
  css += ".skiff-tab[aria-selected=true]{color:" + text + ";font-weight:600}\n";
  css += ".skiff-tabs:focus-within{outline:1px solid " + accent + "}\n";
  css += ".skiff-textbox{height:" + rowH + ";box-sizing:border-box;padding:0 " + padX + ";border:1px solid transparent;border-radius:" + corner +
         ";background:" + surface + ";color:" + text + ";font:" + font + " system-ui,sans-serif;outline:none}\n";
  css += ".skiff-textbox::placeholder{color:" + faint + "}\n";
  css += ".skiff-textbox:focus{border-color:" + accent + "}\n";
  css += ".skiff-select{height:30px;box-sizing:border-box;padding:0 28px 0 " + padX + ";border:1px solid " + active + ";border-radius:" + corner +
         ";background:" + surface + " " + chevron + " no-repeat right 10px center;color:" + text + ";font:" + font +
         " system-ui,sans-serif;appearance:none;-webkit-appearance:none;cursor:pointer}\n";
  css += ".skiff-select:hover{background-color:" + hover + "}\n";
  css += ".skiff-select:focus{border-color:" + accent + ";outline:none}\n";
  css += ".skiff-toggle{appearance:none;-webkit-appearance:none;width:40px;height:22px;border-radius:11px;background:" + surface +
         ";position:relative;margin:0;cursor:pointer;flex:none;transition:background .12s}\n";
  css += ".skiff-toggle::before{content:'';position:absolute;top:3px;left:3px;width:16px;height:16px;border-radius:50%;background:" + text +
         ";transition:left .12s}\n";
  css += ".skiff-toggle:checked{background:" + accent + "}\n";
  css += ".skiff-toggle:checked::before{left:21px}\n";
  css += ".skiff-toggle:focus-visible{outline:1.5px solid " + accent + ";outline-offset:1px}\n";
  css += ".skiff-slider{appearance:none;-webkit-appearance:none;height:16px;background:transparent;margin:0;cursor:pointer;width:100%;min-width:80px;--skiff-track:" +
         surface + "}\n";
  css += ".skiff-slider::-webkit-slider-runnable-track{height:6px;border-radius:3px;background:var(--skiff-track)}\n";
  css += ".skiff-slider::-moz-range-track{height:6px;border-radius:3px;background:" + surface + "}\n";
  css += ".skiff-slider::-moz-range-progress{height:6px;border-radius:3px;background:" + accent + "}\n";
  css += ".skiff-slider::-webkit-slider-thumb{appearance:none;-webkit-appearance:none;width:14px;height:14px;border-radius:50%;background:" + text +
         ";margin-top:-4px}\n";
  css += ".skiff-slider::-moz-range-thumb{width:14px;height:14px;border:0;border-radius:50%;background:" + text + "}\n";
  css += ".skiff-slider:focus-visible{outline:1.5px solid " + accent + ";outline-offset:2px;border-radius:7px}\n";
  return css;
}


// The theme's stylesheet put into the document, before a page is mounted
// under `into`: the element it was put in, to drop or replace.
inline dom::Handle mountTheme(dom::Handle into, const Theme &t = theme()) {
  const dom::Handle style = dom::create("style");
  dom::text(style, themeCss(t));
  dom::append(into, style);
  return style;
}


// A checkbox showing a bool part -- or an optional one, unsaid is off -- and
// setting it to the other when changed.
template <class T = bool> class ToggleField : public Node {
public:
  explicit ToggleField(scene::Spec spec = {}) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "skiff-toggle");
    dom::attr(fState.fHandle, "type", "checkbox");
    dom::listen(fState.fHandle, "change");
  }
  void sync() { dom::checked(fState.fHandle, fNow); }
  void read(const T &now) {
    const bool on = onOf(now);
    if (on != fNow || !fShown) {
      fNow = on;
      markDirty();
    }
    fShown = true;
  }
  [[nodiscard]] bool on() const noexcept { return fNow; }
  auto onPress() { return bind::own(model::setTo(T(!fNow))); }

private:
  static bool onOf(bool now) { return now; }
  static bool onOf(const std::optional<bool> &now) { return now.value_or(false); }
  bool fNow = false;
  bool fShown = false;
};

// A range input showing a number part between two bounds and setting it as
// it is dragged: a whole number rounded to the nearest.
template <class T> class SliderField : public Node {
public:
  SliderField(T low, T high, scene::Spec spec = {}) : fLow(low), fHigh(high) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "skiff-slider");
    dom::attr(fState.fHandle, "type", "range");
    dom::attr(fState.fHandle, "min", "0");
    dom::attr(fState.fHandle, "max", std::to_string(kSteps));
    dom::attr(fState.fHandle, "step", "1");
    dom::listen(fState.fHandle, "input");
  }
  void sync() {
    dom::value(fState.fHandle, std::to_string(static_cast<int>(std::lround(fFraction * kSteps))));
    // The track filled up to the knob, as the scene draws it: the gradient is
    // the element's own, where the browser's track cannot be told the value.
    const std::string at = number(fFraction * 100.0f) + "%";
    dom::style(fState.fHandle, "--skiff-track",
               "linear-gradient(to right," + rgba(fTheme.fAccent) + " 0 " + at + "," + rgba(fTheme.fSurface) + " " + at + " 100%)");
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    markDirty();
  }
  void read(const T &now) {
    const double span = static_cast<double>(fHigh) - static_cast<double>(fLow);
    const float fraction = span == 0.0 ? 0.0f : static_cast<float>((static_cast<double>(now) - static_cast<double>(fLow)) / span);
    if (fraction != fFraction) {
      fFraction = fraction;
      markDirty();
    }
  }
  [[nodiscard]] float fraction() const noexcept { return fFraction; }
  [[nodiscard]] T value() const { return valueAt(fFraction, fLow, fHigh); }
  bool onDom(dom::Handle, std::string_view event, std::string_view value) {
    if (event != "input" && event != "change") return false;
    int steps = 0;
    std::from_chars(value.data(), value.data() + value.size(), steps);
    fFraction = std::clamp(static_cast<float>(steps) / kSteps, 0.0f, 1.0f);
    return true;
  }
  auto onPress() { return bind::own(model::setTo(valueAt(fFraction, fLow, fHigh))); }

private:
  static constexpr int kSteps = 1000;
  template <std::integral N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(std::lround(static_cast<double>(low) + fraction * (static_cast<double>(high) - low)));
  }
  template <std::floating_point N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(low + fraction * (high - low));
  }
  T fLow;
  T fHigh;
  float fFraction = 0.0f;
  Theme fTheme = theme();
};

// A text input showing a string part -- a std::string, or a type whose one
// member is its text -- and setting it as it is typed into.
template <class T = std::string> class TextField : public Node {
public:
  explicit TextField(std::string placeholder = {}, scene::Spec spec = {}) : fPlaceholder(std::move(placeholder)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "skiff-textbox");
    dom::attr(fState.fHandle, "type", "text");
    if (!fPlaceholder.empty()) dom::attr(fState.fHandle, "placeholder", fPlaceholder);
    dom::listen(fState.fHandle, "input");
  }
  void sync() { dom::value(fState.fHandle, fText); }
  void read(const T &now) {
    const std::string &shown = textOf(now);
    if (shown != fText) {
      fText = shown;
      markDirty();
    }
  }
  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  bool onDom(dom::Handle, std::string_view event, std::string_view value) {
    if (event != "input" && event != "change") return false;
    fText = std::string(value);  // the element shows it already
    return true;
  }
  auto onPress() { return bind::own(model::setTo(T{fText})); }

private:
  static const std::string &textOf(const std::string &value) { return value; }
  template <class Wrapped> static const std::string &textOf(const Wrapped &value) { return value.value; }
  std::string fText;
  std::string fPlaceholder;
};

// A row of tabs, one for each value a part can be, showing the one it is and
// setting it to the one clicked.
template <class T> class ChoiceTabs : public Node {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceTabs(std::vector<Choice> choices, scene::Spec spec = {}) : fChoices(std::move(choices)) { fState.apply(spec); }
  void mounted() {
    dom::attr(fState.fHandle, "class", "skiff-tabs");
    dom::attr(fState.fHandle, "role", "tablist");
    fTabs.clear();
    for (const auto &[label, value] : fChoices) {
      const dom::Handle tab = dom::create("button");
      dom::attr(tab, "class", "skiff-tab");
      dom::attr(tab, "role", "tab");
      dom::attr(tab, "type", "button");
      dom::text(tab, label);
      dom::listen(tab, "click");
      dom::append(fState.fHandle, tab);
      fTabs.push_back(tab);
    }
  }
  template <class F> void handles(F &&f) {
    f(fState.fHandle);
    for (dom::Handle tab : fTabs) f(tab);
  }
  void sync() {
    for (std::size_t i = 0; i < fTabs.size(); ++i) dom::attr(fTabs[i], "aria-selected", static_cast<int>(i) == fSelected ? "true" : "false");
  }
  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    const int selected = found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin());
    if (selected != fSelected) {
      fSelected = selected;
      markDirty();
    }
  }
  [[nodiscard]] int selected() const noexcept { return fSelected; }
  [[nodiscard]] std::span<const dom::Handle> tabs() const noexcept { return fTabs; }
  bool onDom(dom::Handle target, std::string_view event, std::string_view) {
    if (event != "click") return false;
    const auto found = std::ranges::find(fTabs, target);
    fPicked = found == fTabs.end() ? -1 : static_cast<int>(found - fTabs.begin());
    return fPicked >= 0;
  }
  auto onPress() -> std::optional<bind::Own<model::SetTo<T>>> {
    if (fPicked < 0 || static_cast<std::size_t>(fPicked) >= fChoices.size()) return std::nullopt;
    return bind::own(model::setTo(fChoices[static_cast<std::size_t>(fPicked)].second));
  }

private:
  std::vector<Choice> fChoices;
  std::vector<dom::Handle> fTabs;
  int fSelected = -1;
  int fPicked = -1;
};

// A select, one option for each value a part can be: the scene's ChoiceList, a dropdown's list.
template <class T> class ChoiceList : public Node {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceList(std::vector<Choice> choices, scene::Spec spec = {}) : fChoices(std::move(choices)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "select"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "skiff-select");
    for (std::size_t i = 0; i < fChoices.size(); ++i) {
      const dom::Handle option = dom::create("option");
      dom::attr(option, "value", std::to_string(i));
      dom::text(option, fChoices[i].first);
      dom::append(fState.fHandle, option);
    }
    dom::listen(fState.fHandle, "change");
  }
  void sync() { dom::value(fState.fHandle, fSelected < 0 ? std::string() : std::to_string(fSelected)); }
  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    const int selected = found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin());
    if (selected != fSelected) {
      fSelected = selected;
      markDirty();
    }
  }
  [[nodiscard]] int selected() const noexcept { return fSelected; }
  bool onDom(dom::Handle, std::string_view event, std::string_view value) {
    if (event != "change" && event != "input") return false;
    int picked = -1;
    std::from_chars(value.data(), value.data() + value.size(), picked);
    fPicked = picked;
    return fPicked >= 0 && static_cast<std::size_t>(fPicked) < fChoices.size();
  }
  auto onPress() -> std::optional<bind::Own<model::SetTo<T>>> {
    if (fPicked < 0 || static_cast<std::size_t>(fPicked) >= fChoices.size()) return std::nullopt;
    return bind::own(model::setTo(fChoices[static_cast<std::size_t>(fPicked)].second));
  }

private:
  std::vector<Choice> fChoices;
  int fSelected = -1;
  int fPicked = -1;
};

// A button sending a copy of its event each time it is clicked, up through
// the scopes it is in, as the press is delivered.
template <class E> class SendButton : public Node {
public:
  using Out = model::Types<E>;
  SendButton(std::string label, E event, scene::Spec spec = {}) : fLabel(std::move(label)), fEvent(std::move(event)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "button"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "skiff-button");
    dom::attr(fState.fHandle, "type", "button");
    dom::listen(fState.fHandle, "click");
  }
  void sync() { dom::text(fState.fHandle, fLabel); }
  void read(E event) { fEvent = std::move(event); }
  void read(std::pair<std::string, E> value) {
    setLabel(std::move(value.first));
    fEvent = std::move(value.second);
  }
  void setLabel(std::string label) {
    if (label != fLabel) {
      fLabel = std::move(label);
      markDirty();
    }
  }
  bool onDom(dom::Handle, std::string_view event, std::string_view) { return event == "click"; }
  E onPress() { return fEvent; }

private:
  std::string fLabel;
  E fEvent;
};


} // namespace skiff::widgets::web
