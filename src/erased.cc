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
import splice;
import skiff.scene;

export namespace skiff::widgets {

// Whether widgets hold their actions erased: as the walks are.
inline constexpr bool kErasedActions = skiff::scene::kErasedWalks;

// A call, whatever it is: made as it would be -- splice::erased_call, held
// in a buffer of its own, never on the heap. Erasure, outside a release
// build only. NoAction is held as nothing, so that it still does not act.
// How large an action may be: a few pointers and a string -- what a
// screen's actions carry. A larger one is an error where it is erased.
inline constexpr std::size_t kActionBytes = 8 * sizeof(void *);
template <class Signature>
class AnyCall;
template <class Result, class... Args>
class AnyCall<Result(Args...)> : public splice::erased_call<Result(Args...), kActionBytes> {
  using Base = splice::erased_call<Result(Args...), kActionBytes>;

public:
  AnyCall() = default;
  explicit AnyCall(skiff::scene::NoAction) {}
  template <class Call>
    requires(!std::same_as<std::remove_cvref_t<Call>, AnyCall> &&
             !std::same_as<std::remove_cvref_t<Call>, skiff::scene::NoAction> && std::invocable<const Call &, Args...>)
  explicit AnyCall(Call call) : Base(std::move(call)) {}
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
