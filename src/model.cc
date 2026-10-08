export module skiff.widgets.model;

import std;
import skiff.scene;
import skiff.model;
import skiff.bind;
export import skiff.widgets.button;
export import skiff.widgets.textbox;
export import skiff.widgets.sliderbar;
export import skiff.widgets.tabbar;
export import skiff.widgets.textarea;
export import skiff.widgets.dropdown;

// skiff's widgets, bound to a skiff.model: a text field showing a part and
// setting it as it is typed into; a button sending an event when pressed.
//
// Each keeps what it did in the action object it already holds and calls --
// its own member, so nothing points back at the widget, which may move (a
// row of a list does) -- and skiff.bind's drain takes it from there:
// takeChanges() for a change of the part shown, takeEvents() for events.

export namespace skiff::widgets {

// What a field was typed into, kept until it is taken.
struct Typed {
  std::optional<std::string> fText;
  void operator()(std::string_view text) {
    fText = std::string(text);
    ++skiff::bind::pendingCount();
  }
};

// A text box showing a part of the model -- a Named<..., std::string>, or
// any type whose one member is its text -- and setting it as it is typed
// into. Wrapped in bind::Bound by compose::bound<T>(TextField<T>(...)).
template <class T> class TextField : public internal::TextBox<Typed> {
public:
  explicit TextField(std::string placeholder = {})
      : internal::TextBox<Typed>(std::move(placeholder), Typed{}) {}
  TextField(Theme theme, std::string placeholder)
      : internal::TextBox<Typed>(std::move(theme), std::move(placeholder), Typed{}) {}

  void read(const T &now) {
    const std::string &shown = textOf(now);
    if (shown != this->text())
      this->setText(shown);
  }
  std::vector<skiff::model::SetTo<T>> takeChanges() {
    std::vector<skiff::model::SetTo<T>> out;
    if (auto typed = std::exchange(this->onChanged().fText, std::nullopt))
      out.push_back(skiff::model::setTo(T{std::move(*typed)}));
    return out;
  }

private:
  static const std::string &textOf(const std::string &value) { return value; }
  template <class Wrapped> static const std::string &textOf(const Wrapped &value) { return value.value; }
};

// The presses of a toggle, counted until they are taken.
struct Pressed {
  int fCount = 0;
  void operator()() {
    ++fCount;
    ++skiff::bind::pendingCount();
  }
};

// A toggle showing a bool part -- a plain field, Field<&Settings::sound> --
// and flipping it when pressed: twice, and it is as it was.
class ToggleField : public internal::Toggle<Pressed> {
public:
  ToggleField() : internal::Toggle<Pressed>(Pressed{}) {}
  explicit ToggleField(Theme theme) : internal::Toggle<Pressed>(std::move(theme), Pressed{}) {}

  void read(bool now) {
    // The state it opens in shown there and then; a change after, slid to.
    if (std::exchange(fShown, true))
      this->setOn(now);
    else
      this->setOnNow(now);
  }
  std::vector<skiff::model::Flip> takeChanges() {
    const int pressed = std::exchange(this->onToggle().fCount, 0);
    return std::vector<skiff::model::Flip>(static_cast<std::size_t>(pressed % 2), skiff::model::flip);
  }

private:
  bool fShown = false;
};

// Where a slider was dragged to, kept until it is taken.
struct Slid {
  std::optional<float> fFraction;
  void operator()(float fraction) {
    fFraction = fraction;
    ++skiff::bind::pendingCount();
  }
};

// A slider showing a number part between two bounds, and setting it as it
// is dragged: a whole number rounded to the nearest.
template <class T> class SliderField : public internal::SliderBar<Slid> {
public:
  SliderField(T low, T high) : internal::SliderBar<Slid>(Slid{}), fLow(low), fHigh(high) {}
  SliderField(Theme theme, T low, T high)
      : internal::SliderBar<Slid>(std::move(theme), Slid{}), fLow(low), fHigh(high) {}

  void read(const T &now) {
    const double span = static_cast<double>(fHigh) - static_cast<double>(fLow);
    this->setFraction(span == 0.0 ? 0.0f
                                  : static_cast<float>((static_cast<double>(now) - static_cast<double>(fLow)) / span));
  }
  std::vector<skiff::model::SetTo<T>> takeChanges() {
    std::vector<skiff::model::SetTo<T>> out;
    if (const auto fraction = std::exchange(this->onSet().fFraction, std::nullopt))
      out.push_back(skiff::model::setTo(valueAt(*fraction, fLow, fHigh)));
    return out;
  }

private:
  template <std::integral N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(std::lround(static_cast<double>(low) + fraction * (static_cast<double>(high) - low)));
  }
  template <std::floating_point N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(low + fraction * (high - low));
  }
  T fLow;
  T fHigh;
};

// The tab picked, kept until it is taken: its place in the row.
struct Picked {
  std::optional<int> fIndex;
  void operator()(int index) {
    fIndex = index;
    ++skiff::bind::pendingCount();
  }
};

// A row of tabs, one for each value a part can be -- a variant's
// alternatives, an enumeration's names -- showing the one it is and setting
// it to the one picked.
template <class T> class ChoiceTabs : public internal::TabBar<Picked> {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceTabs(std::vector<Choice> choices)
      : internal::TabBar<Picked>(Picked{}), fChoices(std::move(choices)) {
    std::vector<typename internal::TabBar<Picked>::Tab> tabs;
    for (std::size_t i = 0; i < fChoices.size(); ++i)
      tabs.push_back({fChoices[i].first, static_cast<int>(i)});
    this->setTabs(std::move(tabs));
  }

  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    this->setSelected(found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin()));
  }
  std::vector<skiff::model::SetTo<T>> takeChanges() {
    std::vector<skiff::model::SetTo<T>> out;
    if (const auto index = std::exchange(this->onSelect().fIndex, std::nullopt);
        index && *index >= 0 && static_cast<std::size_t>(*index) < fChoices.size())
      out.push_back(skiff::model::setTo(fChoices[static_cast<std::size_t>(*index)].second));
    return out;
  }

private:
  std::vector<Choice> fChoices;
};

// A dropdown's rows, one for each value a part can be, showing the one it
// is and setting it to the one chosen.
template <class T> class ChoiceList : public internal::DropdownList<Picked> {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceList(std::vector<Choice> choices)
      : internal::DropdownList<Picked>(Picked{}), fChoices(std::move(choices)) {
    this->setOptions(std::ranges::to<std::vector>(std::views::transform(fChoices, &Choice::first)));
  }

  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    this->setCurrent(found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin()));
  }
  std::vector<skiff::model::SetTo<T>> takeChanges() {
    std::vector<skiff::model::SetTo<T>> out;
    if (const auto index = std::exchange(this->onChoose().fIndex, std::nullopt);
        index && *index >= 0 && static_cast<std::size_t>(*index) < fChoices.size())
      out.push_back(skiff::model::setTo(fChoices[static_cast<std::size_t>(*index)].second));
    return out;
  }

private:
  std::vector<Choice> fChoices;
};

// What a text area sent, kept until it is taken.
template <class E> struct Submitted {
  std::vector<E> fItems;
  void operator()(std::string_view text) {
    fItems.push_back(E{std::string(text)});
    ++skiff::bind::pendingCount();
  }
};

// A text area sending an E made of its text each time it is submitted --
// a message from a composer -- and emptied for the next.
template <class E> class SubmitArea : public internal::TextArea<Submitted<E>> {
public:
  using Out = skiff::model::Types<E>;
  explicit SubmitArea(std::string placeholder = {})
      : internal::TextArea<Submitted<E>>(std::move(placeholder), Submitted<E>{}) {}
  SubmitArea(Theme theme, std::string placeholder)
      : internal::TextArea<Submitted<E>>(std::move(theme), std::move(placeholder), Submitted<E>{}) {}
  std::vector<E> takeEvents() {
    auto sent = std::exchange(this->onSubmit().fItems, {});
    if (!sent.empty())
      this->setText({});
    return sent;
  }
};

// The events a button sent, kept until they are taken.
template <class E> struct Sent {
  E fEvent;
  std::vector<E> fItems;
  void operator()() {
    fItems.push_back(fEvent);
    ++skiff::bind::pendingCount();
  }
};

// A button sending a copy of its event each time it is pressed, up through
// the scopes it is in.
template <class E> class SendButton : public internal::Button<Sent<E>> {
public:
  using Out = skiff::model::Types<E>;
  SendButton(std::string label, E event)
      : internal::Button<Sent<E>>(std::move(label), Sent<E>{std::move(event), {}}) {}
  SendButton(Theme theme, std::string label, E event)
      : internal::Button<Sent<E>>(std::move(theme), std::move(label),
                                  Sent<E>{std::move(event), {}}) {}
  std::vector<E> takeEvents() { return std::exchange(this->action().fItems, {}); }
};

} // namespace skiff::widgets
