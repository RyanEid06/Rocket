#include "language_server.h"

#include <iostream>
#include <string>

#ifndef ROCKET_LSP_VERSION
#define ROCKET_LSP_VERSION "0.1.0"
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
  rocket::LanguageServer server(std::cin, std::cout, std::cerr);
  return server.run();
}
