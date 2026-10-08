import std;
import gtest;
import skia;
import skiff.scene;
import skiff.model;
import skiff.bind;
import skiff.compose;
import skiff.widgets.model;

#include "gtest/gtest-macros.h"

// The widgets bound to a model: what is typed into a field sets its part,
// and the field shows what the part becomes; a button's press is an event
// its scope takes.

namespace {

using namespace skiff;
using namespace skiff::compose;

using Query = model::Named<"query", std::string>;
using Count = model::Named<"count", int>;
struct Root {
  model::Tracked<Query> query;
  model::Tracked<Count> count;
};
struct Reactions {};
using Model = model::Model<Root, Reactions>;

struct Clicked {};

auto page() {
  return scoped<Root>(handlers(handle<Clicked>([](const Clicked &, const auto &) {
                        return model::over<Count>([](Count c) { ++c.value; return c; });
                      })),
                      column(vbox(4), bound<Query>(widgets::TextField<Query>("Search")),
                             widgets::SendButton<Clicked>("Count", Clicked{})));
}

TEST(WidgetsModel, TypingSetsThePartAndTheFieldShowsIt) {
  Model m;
  auto p = page();
  bind::Binding<Model> binding;
  binding.refresh(p, m);
  auto &field = std::get<0>(p.fParts);
  field.onChanged()("bo");  // as typing calls it
  binding.drain(p, m);
  EXPECT_EQ(m.look<Query>()->value, "bo");
  m.apply(model::over<Query>(model::setTo(Query{"bob"})));
  binding.refresh(p, m);
  EXPECT_EQ(field.text(), "bob");
}

// Plain fields of one type, bound by their member pointers: each widget
// sets its own field, and shows what its field becomes.
enum class Look { kSolid, kFrosted };
struct Settings {
  bool sound = false;
  bool popups = true;
  float volume = 0.5f;
  int lines = 3;
  Look look = Look::kSolid;
  std::string name;
  std::optional<bool> push;
};
struct SettingsRoot {
  model::Tracked<Settings> settings;
};
using SettingsModel = model::Model<SettingsRoot, Reactions>;
auto settingsPage() {
  return column(vbox(4), bound<model::Field<&Settings::sound>>(widgets::ToggleField()),
                bound<model::Field<&Settings::popups>>(widgets::ToggleField()),
                bound<model::Field<&Settings::volume>>(widgets::SliderField<float>(0.0f, 1.0f)),
                bound<model::Field<&Settings::lines>>(widgets::SliderField<int>(1, 11)),
                bound<model::Field<&Settings::look>>(widgets::ChoiceTabs<Look>(
                    {{"Solid", Look::kSolid}, {"Frosted", Look::kFrosted}})),
                bound<model::Field<&Settings::name>>(widgets::TextField<std::string>("Name")),
                bound<model::Field<&Settings::push>>(widgets::ToggleField<std::optional<bool>>()));
}
TEST(WidgetsModel, PlainFieldsAreBoundByTheirMemberPointers) {
  SettingsModel m;
  auto p = settingsPage();
  bind::Binding<SettingsModel> binding;
  binding.refresh(p, m);
  auto &[sound, popups, volume, lines, look, name, push] = p.fParts;
  EXPECT_FALSE(sound.on());
  EXPECT_TRUE(popups.on());
  sound.onToggle()();  // as a press calls it
  volume.onSet()(0.25f);
  lines.onSet()(0.5f);
  look.onSelect()(1);
  name.onChanged()("Ann");
  push.onToggle()();  // unsaid, so off: on
  binding.drain(p, m);
  const Settings &now = m.root().settings.fValue;
  EXPECT_TRUE(now.sound);
  EXPECT_TRUE(now.popups);  // its own field, though of the same type
  EXPECT_FLOAT_EQ(now.volume, 0.25f);
  EXPECT_EQ(now.lines, 6);
  EXPECT_EQ(now.look, Look::kFrosted);
  EXPECT_EQ(now.name, "Ann");
  EXPECT_EQ(now.push, std::optional<bool>(true));
  m.apply(model::edit(model::placeOf<model::Field<&Settings::popups>, SettingsRoot>(), model::flip));
  binding.refresh(p, m);
  EXPECT_FALSE(popups.on());
  EXPECT_EQ(look.selected(), 1);
}

// A list's pick sets its part; a text area's text, submitted, is an event
// its scope takes, and the area is emptied.
struct Said {
  std::string text;
};
struct Chat {
  Look look = Look::kSolid;
  std::vector<std::string> said;
};
struct ChatRoot {
  model::Tracked<Chat> chat;
};
using ChatModel = model::Model<ChatRoot, Reactions>;
auto chatPage() {
  return scoped<ChatRoot>(
      handlers(handle<Said>([](const Said &one, const auto &) {
        return model::over<model::Field<&Chat::said>>([text = one.text](std::vector<std::string> all) {
          all.push_back(text);
          return all;
        });
      })),
      column(vbox(4),
             bound<model::Field<&Chat::look>>(widgets::ChoiceList<Look>({{"Solid", Look::kSolid}, {"Frosted", Look::kFrosted}})),
             widgets::SubmitArea<Said>("Message")));
}
TEST(WidgetsModel, AListSetsItsPartAndAnAreaSendsItsText) {
  ChatModel m;
  auto p = chatPage();
  bind::Binding<ChatModel> binding;
  binding.refresh(p, m);
  auto &list = std::get<0>(p.fParts);
  auto &area = std::get<1>(p.fParts);
  EXPECT_EQ(list.current(), 0);
  list.onChoose()(1);
  area.setText("hi");
  area.onSubmit()("hi");  // as Enter calls it
  binding.drain(p, m);
  EXPECT_EQ(m.root().chat.fValue.look, Look::kFrosted);
  ASSERT_EQ(m.root().chat.fValue.said.size(), 1u);
  EXPECT_EQ(m.root().chat.fValue.said[0], "hi");
  EXPECT_EQ(area.text(), "");
}

TEST(WidgetsModel, APressIsAnEventItsScopeTakes) {
  Model m;
  auto p = page();
  bind::Binding<Model> binding;
  binding.refresh(p, m);
  auto &button = std::get<1>(p.fParts);
  // Pressed: it says so, and keeps nothing.
  button.fState.fBounds = skia::SkRect::MakeWH(80, 30);
  ASSERT_TRUE(button.onClick(10, 10));
  ASSERT_EQ(scene::hostWork().pressed.size(), 1u);
  EXPECT_EQ(scene::hostWork().pressed.back(), &button.fState);
  scene::hostWork().pressed.clear();
  // Delivered along its path -- the page.s second part -- twice: its
  // scope takes the event each time.
  EXPECT_TRUE(bind::press(p, m, {button.fState.fId}));
  EXPECT_TRUE(bind::press(p, m, {button.fState.fId}));
  EXPECT_EQ(m.look<Count>()->value, 2);
}

} // namespace
