import std;
import gtest;
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

TEST(WidgetsModel, APressIsAnEventItsScopeTakes) {
  Model m;
  auto p = page();
  bind::Binding<Model> binding;
  binding.refresh(p, m);
  auto &button = std::get<1>(p.fParts);
  button.action()();  // as a press calls it
  button.action()();
  binding.drain(p, m);
  EXPECT_EQ(m.look<Count>()->value, 2);
}

} // namespace
