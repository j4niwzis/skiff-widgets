// SPDX-License-Identifier: AGPL-3.0-only
import std;
import gtest;
import skiff.core;
import skiff.model;
import skiff.bind;
import skiff.web;
import skiff.widgets.web;

#include "gtest/gtest-macros.h"

// The page's controls: each showing its part of the model, dressed in the
// theme; what is done to a control sets its part, and what the model says
// is written back to the page.

namespace {
using namespace skiff;
using namespace skiff::web;
using namespace skiff::widgets::web;

enum class Look { kSolid, kFrosted };
struct Settings {
  bool sound = false;
  float volume = 0.5f;
  int lines = 3;
  Look look = Look::kSolid;
  std::string name;
};
struct Root {
  model::Tracked<Settings> settings;
};
struct Lines {
  std::vector<std::string> all;
};
struct Count2 {
  model::Tracked<Lines> lines;
};
struct Said2 {
  std::string text;
};
struct Reactions {};
using Model = model::Model<Root, Reactions>;
using Model2 = model::Model<Count2, Reactions>;

auto settingsPage() {
  return column(vbox(8.0f, {.padding = scene::Margin::all(12.0f)}),
                row(hbox(8.0f), Text("Sound"), bound<model::Field<&Settings::sound>>(ToggleField())),
                row(hbox(8.0f), Text("Volume"), bound<model::Field<&Settings::volume>>(SliderField<float>(0.0f, 1.0f))),
                bound<model::Field<&Settings::lines>>(SliderField<int>(1, 11)),
                bound<model::Field<&Settings::look>>(ChoiceTabs<Look>({{"Solid", Look::kSolid}, {"Frosted", Look::kFrosted}})),
                bound<model::Field<&Settings::name>>(TextField<std::string>("Name")),
                bound<model::Field<&Settings::look>>(ChoiceList<Look>({{"Solid", Look::kSolid}, {"Frosted", Look::kFrosted}})));
}
using PageTree = decltype(settingsPage());

TEST(Web, TheTreeIsMountedAsElementsShowingTheModel) {
  dom::reset();
  Model m;
  Page page(settingsPage(), m);
  mountTheme(dom::root());
  page.mount();
  const std::string html = dom::dump(dom::root());
  EXPECT_NE(html.find("<input class=\"skiff-toggle\" type=\"checkbox\""), std::string::npos) << html;
  EXPECT_NE(html.find("type=\"range\""), std::string::npos);
  EXPECT_NE(html.find("value=\"500\""), std::string::npos) << html;       // volume 0.5 of 1000
  EXPECT_NE(html.find("role=\"tablist\""), std::string::npos);
  EXPECT_NE(html.find("aria-selected=\"true\""), std::string::npos);
  EXPECT_NE(html.find("<select"), std::string::npos);
  EXPECT_NE(html.find("display:flex;"), std::string::npos);
  EXPECT_NE(html.find("padding:12px 12px 12px 12px;"), std::string::npos) << html;
  EXPECT_NE(html.find("<span class=\"skiff-text\">Sound</span>"), std::string::npos) << html;
  EXPECT_NE(html.find("<style>.skiff-text{"), std::string::npos) << html.substr(0, 200);
}

TEST(Web, WhatIsDoneToAControlSetsItsPartAndThePageShowsIt) {
  dom::reset();
  Model m;
  Page page(settingsPage(), m);
  mountTheme(dom::root());
  page.mount();
  auto &sound = std::get<1>(std::get<0>(page.root().fParts).fParts);
  auto &volume = std::get<1>(std::get<1>(page.root().fParts).fParts);
  auto &look = std::get<3>(page.root().fParts);
  auto &name = std::get<4>(page.root().fParts);
  auto &pick = std::get<5>(page.root().fParts);

  fire(page, sound.element(), "change", "");
  EXPECT_TRUE(m.look<Settings>()->sound);
  EXPECT_TRUE(sound.on());

  fire(page, volume.element(), "input", "250");
  EXPECT_NEAR(m.look<Settings>()->volume, 0.25f, 0.001f);
  EXPECT_NE(dom::dump(volume.element()).find("value=\"250\""), std::string::npos);

  fire(page, look.tabs()[1], "click", "");
  EXPECT_EQ(m.look<Settings>()->look, Look::kFrosted);
  EXPECT_EQ(look.selected(), 1);
  EXPECT_EQ(pick.selected(), 1);  // the select shows the same part
  EXPECT_NE(dom::dump(look.tabs()[1]).find("aria-selected=\"true\""), std::string::npos);

  fire(page, name.element(), "input", "bob");
  EXPECT_EQ(m.look<Settings>()->name, "bob");

  fire(page, pick.element(), "change", "0");
  EXPECT_EQ(m.look<Settings>()->look, Look::kSolid);
  EXPECT_EQ(look.selected(), 0);
}

TEST(Web, AnEditMadeElsewhereIsWrittenToThePage) {
  dom::reset();
  Model m;
  Page page(settingsPage(), m);
  mountTheme(dom::root());
  page.mount();
  auto &volume = std::get<1>(std::get<1>(page.root().fParts).fParts);
  m.apply(model::over<Settings>([](Settings s) {
    s.volume = 1.0f;
    s.name = "loud";
    return s;
  }));
  page.refresh();
  EXPECT_NE(dom::dump(volume.element()).find("value=\"1000\""), std::string::npos);
  auto &name = std::get<4>(page.root().fParts);
  EXPECT_EQ(name.text(), "loud");
  EXPECT_NE(dom::dump(name.element()).find("value=\"loud\""), std::string::npos);
}

struct Clicked {};
struct Count {
  int n = 0;
};
struct CountRoot {
  model::Tracked<Count> count;
};
using CountModel = model::Model<CountRoot, Reactions>;

// The scene's widgets on the page, each given an action of the test's own.
struct Said {
  int clicks = 0;
  bool on = false;
  std::string typed, sent;
  float fraction = -1.0f, done = -1.0f;
  std::size_t chosen = 99;
};
struct Shot {
  std::shared_ptr<scene::Picture> fPicture;
  const scene::Picture *operator()() const { return fPicture.get(); }
};
auto widgetsPage(Said &said) {
  return column(vbox(8.0f), Button("Go", [&said] { ++said.clicks; }), Toggle([&said](bool on) { said.on = on; }),
                TextBox("Search", [&said](std::string_view text) { said.typed = std::string(text); }),
                SliderBar([&said](float at) { said.fraction = at; }, [&said](float at) { said.done = at; }),
                ChoiceMenu(widgets::theme(), "Sort", std::vector<std::string>{"Artist", "Title"}, 1, [&said](std::size_t index) { said.chosen = index; }),
                Pill("new", Pill::Look{.plate = 0xff336699u}), Avatar("AB", 40.0f, Shot{std::make_shared<scene::Picture>()}, scene::Gradient{0xff000000u, 0xffffffffu}),
                RadialLoader(32.0f), Collapsible<Text>("more"));
}

TEST(Web, TheScenesWidgetsAreTheBrowsersControlsCallingTheirActions) {
  dom::reset();
  Said said;
  Model m;
  Page page(widgetsPage(said), m);
  page.mount();
  std::string html = dom::dump(dom::root());
  EXPECT_NE(html.find("<button class=\"skiff-button\" type=\"button\">Go</button>"), std::string::npos) << html;
  EXPECT_NE(html.find("<input class=\"skiff-toggle\" type=\"checkbox\""), std::string::npos) << html;
  EXPECT_NE(html.find("placeholder=\"Search\""), std::string::npos) << html;
  EXPECT_NE(html.find("<label class=\"skiff-menu\""), std::string::npos) << html;
  EXPECT_NE(html.find("<span class=\"skiff-pill\""), std::string::npos) << html;
  EXPECT_NE(html.find("class=\"skiff-avatar\""), std::string::npos) << html;
  EXPECT_NE(html.find("class=\"skiff-loader\""), std::string::npos) << html;
  // Closed, the collapsible's child is not shown.
  auto &more = std::get<8>(page.root().fParts);
  EXPECT_NE(dom::dump(more.element()).find("display:none;"), std::string::npos) << dom::dump(more.element());
  more.setOpen(true);
  page.sync();
  EXPECT_EQ(dom::dump(more.element()).find("display:none"), std::string::npos) << dom::dump(more.element());

  auto &button = std::get<0>(page.root().fParts);
  fire(page, button.element(), "click", "");
  EXPECT_EQ(said.clicks, 1);
  button.setPrimary(true);
  page.sync();
  EXPECT_NE(dom::dump(button.element()).find("class=\"skiff-button skiff-primary\""), std::string::npos) << dom::dump(button.element());

  auto &toggle = std::get<1>(page.root().fParts);
  fire(page, toggle.element(), "change", "");
  EXPECT_TRUE(said.on);
  EXPECT_TRUE(toggle.on());

  auto &box = std::get<2>(page.root().fParts);
  fire(page, box.element(), "input", "bob");
  EXPECT_EQ(said.typed, "bob");
  EXPECT_EQ(box.text(), "bob");
  box.setMasked(true);
  page.sync();
  EXPECT_NE(dom::dump(box.element()).find("type=\"password\""), std::string::npos);

  auto &bar = std::get<3>(page.root().fParts);
  fire(page, bar.element(), "input", "250");
  EXPECT_NEAR(said.fraction, 0.25f, 0.001f);
  EXPECT_FLOAT_EQ(said.done, -1.0f);
  fire(page, bar.element(), "change", "500");
  EXPECT_NEAR(said.done, 0.5f, 0.001f);

  auto &menu = std::get<4>(page.root().fParts);
  dom::Handle select = dom::kNone;
  menu.handles([&](dom::Handle h) { if (h != menu.element()) select = h; });
  ASSERT_NE(select, dom::kNone);
  fire(page, select, "change", "0");
  EXPECT_EQ(said.chosen, 0u);
  EXPECT_EQ(menu.current, 0u);
}

TEST(Web, ADrawerAndASlideOverShowWhatIsOpenedOverTheirBase) {
  dom::reset();
  Model m;
  Page page(column(Drawer<Text, Text>(std::piecewise_construct, std::make_tuple("base"), std::make_tuple("panel")), SlideOver<Text, Text>("under")), m);
  page.mount();
  auto &drawer = std::get<0>(page.root().fParts);
  auto &over = std::get<1>(page.root().fParts);
  std::string html = dom::dump(drawer.element());
  EXPECT_NE(html.find("class=\"skiff-scrim\" style=\"display:none;\""), std::string::npos) << html;
  EXPECT_NE(html.find("transform:translateX(-100%);"), std::string::npos) << html;
  drawer.open();
  page.sync();
  html = dom::dump(drawer.element());
  EXPECT_TRUE(drawer.isOpen());
  EXPECT_EQ(html.find("display:none"), std::string::npos) << html;
  EXPECT_NE(html.find("transform:translateX(0);"), std::string::npos) << html;
  // The scrim clicked: pushed back.
  dom::Handle scrim = dom::kNone;
  drawer.handles([&](dom::Handle h) { if (h != drawer.element()) scrim = h; });
  fire(page, scrim, "click", "");
  EXPECT_FALSE(drawer.isOpen());
  // A panel opened over the base, and taken back.
  EXPECT_EQ(over.shown(), nullptr);
  over.open("detail");
  page.sync();
  html = dom::dump(over.element());
  EXPECT_NE(html.find(">detail</span>"), std::string::npos) << html;
  EXPECT_NE(html.find("position:absolute;"), std::string::npos) << html;
  ASSERT_NE(over.shown(), nullptr);
  EXPECT_EQ(over.shown()->text(), "detail");
  over.back();
  page.sync();
  EXPECT_EQ(over.shown(), nullptr);
  EXPECT_EQ(dom::dump(over.element()).find(">detail<"), std::string::npos) << dom::dump(over.element());
}

TEST(Web, AnAreaSubmitsOnEnterAndIsEmptiedForTheNext) {
  dom::reset();
  Model2 m;
  Page page(scoped<Count2>(handlers(handle<Said2>([](const Said2 &sent, const auto &) {
                             return model::over<Lines>([sent](Lines l) {
                               l.all.push_back(sent.text);
                               return l;
                             });
                           })),
                           column(SubmitArea<Said2>("Write"))),
            m);
  page.mount();
  auto &area = std::get<0>(page.root().fParts);
  fire(page, area.element(), "input", "hello");
  EXPECT_EQ(area.text(), "hello");
  fire(page, area.element(), "keydown", "Enter|10");  // Shift+Enter: a line, not a submission
  EXPECT_EQ(m.look<Lines>()->all.size(), 0u);
  fire(page, area.element(), "keydown", "Enter|00");
  ASSERT_EQ(m.look<Lines>()->all.size(), 1u);
  EXPECT_EQ(m.look<Lines>()->all[0], "hello");
  EXPECT_TRUE(area.text().empty());
  EXPECT_NE(dom::dump(area.element()).find("<textarea class=\"skiff-textarea\""), std::string::npos) << dom::dump(area.element());
}

TEST(Web, AButtonSendsItsEventToTheScopeAroundIt) {
  dom::reset();
  CountModel m;
  Page page(scoped<CountRoot>(handlers(handle<Clicked>([](const Clicked &, const auto &) {
                                return model::over<Count>([](Count c) {
                                  ++c.n;
                                  return c;
                                });
                              })),
                              column(SendButton<Clicked>("Count", Clicked{}))),
            m);
  page.mount();
  auto &button = std::get<0>(page.root().fParts);
  EXPECT_NE(dom::dump(button.element()).find("<button class=\"skiff-button\" type=\"button\">Count</button>"), std::string::npos) << dom::dump(dom::root());
  fire(page, button.element(), "click", "");
  fire(page, button.element(), "click", "");
  EXPECT_EQ(m.look<Count>()->n, 2);
}

} // namespace
