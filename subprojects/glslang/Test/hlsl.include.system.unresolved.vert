// Should fail even though "./foo.h" exists
#include <foo.h>

int main() {
#error in main
}
