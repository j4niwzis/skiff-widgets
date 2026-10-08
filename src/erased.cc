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
// An action that keeps the events it sent (fEmitted), for a walk to take:
// never erased, so that the walk can reach them.
template <class... Actions>
concept KeepsEvents = (requires(Actions &a) { a.fEmitted; } || ...);

// A call, whatever it is: made as it would be -- spl::erased_call, held
// in a buffer of its own, never on the heap. Erasure, outside a release
// build only. NoAction is held as nothing, so that it still does not act.
// The least an erased call holds: a few pointers and a string -- what a
// screen's actions mostly carry. A larger one is held in a larger AnyCall
// (kBytesOf).
inline constexpr std::size_t kActionBytes = 8 * sizeof(void *);
template <class Signature, std::size_t Bytes = kActionBytes>
class AnyCall;
template <class Result, class... Args, std::size_t Bytes>
class AnyCall<Result(Args...), Bytes> : public spl::erased_call<Result(Args...), Bytes> {
  using Base = spl::erased_call<Result(Args...), Bytes>;

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
// What a call is erased into: an AnyCall as large as the smallest power of
// two that holds it, kActionBytes at least -- so that any call fits, none is
// put on the heap, and a widget is made once for each of a few sizes, not
// once for each call.
template <class Call>
inline constexpr std::size_t kBytesOf = std::bit_ceil(std::max(kActionBytes, sizeof(spl::holder<Call>)));
template <class Signature, class Call>
using AnyCallFor = AnyCall<Signature, kBytesOf<Call>>;
template <class Action>
using AnyActionFor = AnyCallFor<void(), Action>;

} // namespace skiff::widgets

// Whether an erased call acts: where it holds something, NoAction not.
export namespace skiff::scene {
template <class Signature, std::size_t Bytes>
[[nodiscard]] bool acts(const skiff::widgets::AnyCall<Signature, Bytes> &call) noexcept {
  return call.holds();
}
} // namespace skiff::scene
