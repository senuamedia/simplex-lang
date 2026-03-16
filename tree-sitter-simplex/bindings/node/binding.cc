#include <napi.h>

typedef struct TSLanguage TSLanguage;

extern "C" TSLanguage *tree_sitter_simplex();

// "tree-sitter", "currentVersion" returns a number for the node-tree-sitter
// ABI version.
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports["name"] = Napi::String::New(env, "simplex");
  auto language = Napi::External<TSLanguage>::New(env, tree_sitter_simplex());
  exports["language"] = language;
  return exports;
}

NODE_API_MODULE(tree_sitter_simplex_binding, Init)
