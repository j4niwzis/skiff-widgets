// The page in a browser: built with Emscripten, index.html.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
import std;
import skiff.ui;
#include "page.h"

namespace {
Model gModel;
using Tree = decltype(page());
std::optional<Page<Tree, Model>> gPage;
} // namespace

#ifdef __EMSCRIPTEN__
// The document's listeners' way in: the page's type known here, nothing erased.
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_web_event(std::uint32_t handle, const char *event, const char *value) {
  if (gPage) gPage->event(handle, std::string_view(event), std::string_view(value));
}
#endif

int main() {
  mountTheme(web::dom::root());
  gPage.emplace(page(), gModel);
  gPage->mount();
#ifndef __EMSCRIPTEN__
  // Built with the in-memory document: the page as HTML, to look at or test.
  std::println("{}", web::dom::dump(web::dom::root()));
#endif
  return 0;
}
