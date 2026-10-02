#include <iostream>
#include "service/LibraryService.h"
#include "ui/ConsoleApp.h"

int main() {
    titans::LibraryService service;
    titans::ConsoleApp app(service, std::cin, std::cout);
    return app.run();
}
