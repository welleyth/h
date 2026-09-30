#include "../../eolymp.h"

int main() {
    eo::pattern const letters("[a-z");
    return letters.matches("a") ? 0 : 1;
}
