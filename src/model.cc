export module skiff.widgets.model;

import std;
import skiff.scene;
import skiff.model;
import skiff.bind;
export import skiff.widgets.button;
export import skiff.widgets.textbox;

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
  static const std::string &textOf(const T &value) { return value.value; }
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
