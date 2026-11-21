#include <peglib.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <any>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;
using json = nlohmann::json;

using json = nlohmann::json;
const char* grammar = R"(
Document   <- (Heading / Filename / Section / Blank / Paragraph)* !.

Heading    <- '# ' Text End
Filename   <- '! ' Text End
Section    <- ': ' Text End
Paragraph  <- Text End
Blank      <- Newline

Text       <- (Bold / Italic / Underline / Plain)+

Bold       <- '*' BoldText '*'
Italic     <- '/' ItalicText '/'
Underline  <- '_' UnderlineText '_'

BoldText      <- (!'*' !Newline .)+
ItalicText    <- (!'/' !Newline .)+
UnderlineText <- (!'_' !Newline .)+

Plain      <- (!'*' !'/' !'_' !Newline .)+

End        <- Newline / !.
Newline    <- '\r\n' / '\n'
)";

enum class Style {
  Plain,
  Bold,
  Italic,
  Underline
};

struct Span {
  std::string text;
  Style style;
};

enum class BlockType {
  Heading,
  Filename,
  Section,
  Paragraph
};

struct Block {
  BlockType type;
  std::vector<Span> spans;
};

using Document = std::vector<Block>;

std::string read_file(const std::string& path) {
  std::ifstream file(path);

  if (!file) {
    throw std::runtime_error("Cannot open " + path);
  }

  return {
    std::istreambuf_iterator<char>(file),
    std::istreambuf_iterator<char>()
  };
}

Document parse_book(
    const std::string& source,
    const std::string& grammar
    ) {
  peg::parser parser;

  parser.set_logger([](
        size_t line,
        size_t column,
        const std::string& message,
        const std::string& rule
        ) {
      std::cerr << "PEG error at "
      << line << ":" << column
      << ": " << message
      << " (" << rule << ")\n";
      });

  if (!parser.load_grammar(grammar)) {
    throw std::runtime_error("Invalid PEG grammar");
  }

  if (!parser) {
    throw std::runtime_error("Invalid PEG grammar");
  }

  parser["Plain"] = [](const peg::SemanticValues& sv) {
    return Span{sv.token_to_string(), Style::Plain};
  };

  parser["Bold"] = [](const peg::SemanticValues& sv) {
    auto s = sv.token_to_string();
    return Span{s.substr(1, s.size() - 2), Style::Bold};
  };

  parser["Italic"] = [](const peg::SemanticValues& sv) {
    auto s = sv.token_to_string();
    return Span{s.substr(1, s.size() - 2), Style::Italic};
  };

  parser["Underline"] = [](const peg::SemanticValues& sv) {
    auto s = sv.token_to_string();
    return Span{s.substr(1, s.size() - 2), Style::Underline};
  };

  parser["Text"] = [](const peg::SemanticValues& sv) {
    std::vector<Span> spans;

    for (const auto& value : sv) {
      spans.push_back(std::any_cast<Span>(value));
    }

    return spans;
  };

  parser["Heading"] = [](const peg::SemanticValues& sv) {
    return Block{
      BlockType::Heading,
        std::any_cast<std::vector<Span>>(sv[0])
    };
  };

  parser["Filename"] = [](const peg::SemanticValues& sv) {
    return Block{
      BlockType::Filename,
        std::any_cast<std::vector<Span>>(sv[0])
    };
  };

  parser["Section"] = [](const peg::SemanticValues& sv) {
    return Block{
      BlockType::Section,
        std::any_cast<std::vector<Span>>(sv[0])
    };
  };

  parser["Paragraph"] = [](const peg::SemanticValues& sv) {
    return Block{
      BlockType::Paragraph,
        std::any_cast<std::vector<Span>>(sv[0])
    };
  };

  parser["Document"] = [](const peg::SemanticValues& sv) {
    Document document;

    for (const auto& value : sv) {
      if (auto block = std::any_cast<Block>(&value)) {
        document.push_back(*block);
      }
    }

    return document;
  };

  Document document;

  if (!parser.parse(source, document)) {
    throw std::runtime_error("Failed to parse book");
  }

  return document;
}

ftxui::Element renderSpan(const Span& span) {
  auto e = text(span.text);

  switch (span.style) {
    case Style::Bold:      return e | bold;
    case Style::Italic:    return e | italic;
    case Style::Underline: return e | underlined;
    default:               return e;
  }
}

Element renderBlock(const Block& block) {
  Elements spans;

  for (const auto& span : block.spans) {
    spans.push_back(renderSpan(span));
  }

  auto line = hbox(std::move(spans));

  switch (block.type) {
    case BlockType::Heading:
      return line | bold;

    case BlockType::Section:
      return line | underlined;

    case BlockType::Filename:
      return line | dim;

    case BlockType::Paragraph:
      return line;
  }

  return line;
}



int main() {
  auto config = json::parse(read_file("config.json"));

  std::string bookfile = config.value(
    "bookfile",
    std::string("Bookfile.bd")
  );

  if (bookfile == "default") {
    bookfile = "Bookfile.bd";
  }

  std::vector<std::string> files = {bookfile};

  if (config.contains("files") && config["files"].is_array()) {
    for (const auto& file : config["files"]) {
      std::string path = file.get<std::string>();

      if (std::find(files.begin(), files.end(), path) == files.end()) {
        files.push_back(path);
      }
    }
  }

  Document document;
  std::vector<std::string> chapters;
  std::vector<int> chapterPositions;

  std::string openedFile = bookfile;
  std::string search;
  std::vector<int> filteredFiles;

  int selectedFile = 0;
  int selectedChapter = 0;
  int scroll = 0;

  bool pickerOpen = false;
  bool sidebarFocused = true;

  auto updateFilter = [&] {
    filteredFiles.clear();

    std::string query = search;
    std::transform(query.begin(), query.end(), query.begin(),
      [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });

    for (int i = 0; i < static_cast<int>(files.size()); ++i) {
      std::string name = files[i];

      std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) {
          return static_cast<char>(std::tolower(c));
        });

      if (name.find(query) != std::string::npos) {
        filteredFiles.push_back(i);
      }
    }

    selectedFile = 0;
  };

  auto loadBook = [&](int index) {
    Document newDocument = parse_book(
      read_file(files[index]),
      grammar
    );

    std::vector<std::string> newChapters;
    std::vector<int> newPositions;

    for (int i = 0; i < static_cast<int>(newDocument.size()); ++i) {
      if (newDocument[i].type != BlockType::Heading) {
        continue;
      }

      std::string title;

      for (const auto& span : newDocument[i].spans) {
        title += span.text;
      }

      newChapters.push_back(title);
      newPositions.push_back(i);
    }

    document = std::move(newDocument);
    chapters = std::move(newChapters);
    chapterPositions = std::move(newPositions);

    openedFile = files[index];
    selectedChapter = 0;
    scroll = 0;
  };

  loadBook(0);
  updateFilter();

  auto screen = ScreenInteractive::Fullscreen();

  auto renderer = Renderer([&] {
    Elements chapterElements;

    for (int i = 0; i < static_cast<int>(chapters.size()); ++i) {
      auto item = text(chapters[i]);

      if (i == selectedChapter) {
        item = item | inverted;
      }

      chapterElements.push_back(item);
    }

    auto sidebar = vbox({
      text("Contents") | bold | center,
      separator(),
      vbox(std::move(chapterElements)) | frame | flex
    })
      | size(WIDTH, EQUAL, 25)
      | border
      | (sidebarFocused ? color(Color::Cyan) : nothing);

    Elements bookElements;

    for (int i = 0; i < static_cast<int>(document.size()); ++i) {
      auto element = renderBlock(document[i]);

      if (i == scroll) {
        element = element | focus;
      }

      bookElements.push_back(element);
    }

    auto reader = vbox(std::move(bookElements))
      | vscroll_indicator
      | frame
      | flex
      | border
      | (!sidebarFocused ? color(Color::Cyan) : nothing);

    Element top;

    if (pickerOpen) {
      Elements results;

      for (int i = 0; i < static_cast<int>(filteredFiles.size()); ++i) {
        auto item = text(files[filteredFiles[i]]);

        if (i == selectedFile) {
          item = item | inverted | focus;
        }

        results.push_back(item);
      }

      if (results.empty()) {
        results.push_back(text("No matching files") | dim);
      }

      top = vbox({
        text("Search files: " + search + "_") | bold,
        separator(),
        vbox(std::move(results))
          | frame
          | size(HEIGHT, LESS_THAN, 8)
      }) | border;
    } else {
      top = hbox({
        text(" File: ") | bold,
        text(openedFile),
        text("  [/] Search files") | dim
      }) | border;
    }

    return vbox({
      text("Boook") | bold | center,
      top,

      hbox({
        sidebar,
        reader | flex
      }) | flex,

      text(
        pickerOpen
          ? "Type to search | Up/Down Select | Enter Open | Esc Cancel"
          : "Left/Right Switch | Up/Down Navigate | / Files | Q Quit"
      ) | dim | center
    }) | flex;
  });

  auto component = CatchEvent(renderer, [&](Event event) {
    if (pickerOpen) {
      if (event == Event::Escape) {
        pickerOpen = false;
        search.clear();
        return true;
      }

      if (event == Event::Return) {
        if (!filteredFiles.empty()) {
          loadBook(filteredFiles[selectedFile]);
          pickerOpen = false;
          search.clear();
        }
        return true;
      }

      if (event == Event::ArrowDown) {
        if (!filteredFiles.empty()) {
          selectedFile = std::min(
            selectedFile + 1,
            static_cast<int>(filteredFiles.size()) - 1
          );
        }
        return true;
      }

      if (event == Event::ArrowUp) {
        selectedFile = std::max(0, selectedFile - 1);
        return true;
      }

      if (event == Event::Backspace) {
        if (!search.empty()) {
          search.pop_back();
          updateFilter();
        }
        return true;
      }

      if (event.is_character()) {
        search += event.character();
        updateFilter();
        return true;
      }

      return false;
    }

    if (event == Event::Character('/')) {
      pickerOpen = true;
      search.clear();
      updateFilter();
      return true;
    }

    if (event == Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }

    if (event == Event::ArrowLeft) {
      sidebarFocused = true;
      return true;
    }

    if (event == Event::ArrowRight) {
      sidebarFocused = false;
      return true;
    }

    if (sidebarFocused) {
      if (event == Event::ArrowDown && !chapters.empty()) {
        selectedChapter = std::min(
          selectedChapter + 1,
          static_cast<int>(chapters.size()) - 1
        );
        return true;
      }

      if (event == Event::ArrowUp) {
        selectedChapter = std::max(0, selectedChapter - 1);
        return true;
      }

      if (event == Event::Return && !chapterPositions.empty()) {
        scroll = chapterPositions[selectedChapter];
        sidebarFocused = false;
        return true;
      }
    } else {
      int maxScroll = std::max(
        0,
        static_cast<int>(document.size()) - 1
      );

      if (event == Event::ArrowDown) {
        scroll = std::min(scroll + 1, maxScroll);
        return true;
      }

      if (event == Event::ArrowUp) {
        scroll = std::max(0, scroll - 1);
        return true;
      }

      if (event == Event::PageDown) {
        scroll = std::min(scroll + 10, maxScroll);
        return true;
      }

      if (event == Event::PageUp) {
        scroll = std::max(0, scroll - 10);
        return true;
      }
    }

    return false;
  });

  screen.Loop(component);
  return 0;
}

