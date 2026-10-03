// skiff.widgets.erased -- What a widget is told to do, held erased outside a
// release build: each widget is then one class for every action it is given,
// made once here, not again in every program unit that names it with its
// own. A release build keeps each action's own type, all of it static.
//
// A widget's public name is an alias: in a release build to the widget
// itself, made for its action; otherwise to a thin class over the widget
// made for AnyAction, whose only code of its own takes the action given and
// erases it. What is written with either is the same.
export module skiff.widgets.erased;

import std;
import skiff.scene;

export namespace skiff::widgets {

// Whether widgets hold their actions erased: as the walks are.
inline constexpr bool kErasedActions = skiff::scene::kErasedWalks;

// A call, whatever it is: made as it would be. Erasure, outside a release
// build only. NoAction is held as nothing, so that it still does not act.
template <class Signature>
class AnyCall;
template <class... Args>
class AnyCall<void(Args...)> {
public:
  AnyCall() = default;
  explicit AnyCall(skiff::scene::NoAction) {}
  template <class Call>
    requires(!std::same_as<std::remove_cvref_t<Call>, AnyCall> &&
             !std::same_as<std::remove_cvref_t<Call>, skiff::scene::NoAction> && std::invocable<const Call &, Args...>)
  explicit AnyCall(Call call) : fCall(std::move(call)) {}
  void operator()(Args... args) const {
    if (fCall) {
      fCall(std::forward<Args>(args)...);
    }
  }
  [[nodiscard]] bool holds() const noexcept { return static_cast<bool>(fCall); }

private:
  std::function<void(Args...)> fCall;
};
// An action: a call of nothing.
using AnyAction = AnyCall<void()>;

} // namespace skiff::widgets

// Whether an erased call acts: where it holds something, NoAction not.
export namespace skiff::scene {
template <class Signature>
[[nodiscard]] bool acts(const skiff::widgets::AnyCall<Signature> &call) noexcept {
  return call.holds();
}
} // namespace skiff::scene
