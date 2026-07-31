#include "language_server.h"

#include <iostream>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#ifndef ROCKET_LSP_VERSION
#define ROCKET_LSP_VERSION "1.0.0"
#endif

int main(int argc, char** argv) {
  if (argc == 2 && (std::string(argv[1]) == "--version" ||
                    std::string(argv[1]) == "version")) {
    std::cout << "rocket-lsp " ROCKET_LSP_VERSION "\n";
    return 0;
  }
  if (argc == 2 && (std::string(argv[1]) == "--help" ||
                    std::string(argv[1]) == "-h" ||
                    std::string(argv[1]) == "help")) {
    std::cout << "Rocket language server " ROCKET_LSP_VERSION "\n"
                 "usage: rocket-lsp [--version]\n"
                 "The server uses LSP 3.17 Content-Length framing over stdio.\n";
    return 0;
  }
  if (argc != 1) {
    std::cerr << "rocket-lsp: unexpected command-line arguments\n";
    return 2;
  }
#ifdef _WIN32
  // LSP framing counts bytes and writes explicit CRLF separators. Windows text
  // mode would rewrite those bytes and corrupt real editor pipes.
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  rocket::LanguageServer server(std::cin, std::cout, std::cerr);
  return server.run();
}
