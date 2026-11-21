add_rules("mode.debug", "mode.release")

add_requires("pkgconfig::ftxui", {
  system = true
})

add_requires("nlohmann_json")

target("boook")
  set_kind("binary")
  set_languages("cxx17")
  set_rundir("$(projectdir)")

  add_files("src/*.cpp")
  add_includedirs("include")

  add_packages(
    "pkgconfig::ftxui",
    "nlohmann_json"
  )

