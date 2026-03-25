#include "application.hpp"

int main() {
    app::Application application;
    if (!application.init()) {
        return 1;
    }

    application.run();
    application.shutdown();
    return 0;
}