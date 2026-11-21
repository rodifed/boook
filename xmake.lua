add_rules("mode.debug", "mode.release")

add_requires("pkgconfig::ftxui-component", {
  system = true
})

add_requires("pkgconfig::ftxui-dom", {
  system = true
})

add_requires("pkgconfig::ftxui-screen", {
  system = true
})

add_requires("nlohmann_json")

target("boook")
  set_kind("binary")
  set_languages("cxx17")

  add_files("src/*.cpp")
  add_includedirs("include")

  add_packages(
    "pkgconfig::ftxui-component",
    "pkgconfig::ftxui-dom",
    "pkgconfig::ftxui-screen",
    "nlohmann_json"
  )
