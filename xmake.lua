add_rules("mode.debug", "mode.release")

add_requires("ftxui", "nlohmann_json")

target("boook")
  set_kind("binary")
  set_languages("cxx17")

  add_files("src/*.cpp")
  add_includedirs("include")

  add_packages("ftxui", "nlohmann_json")
