// v2.87 current regression ownership.
// Shared Microsoft symbol-display helper golden tests.
#include "label-symbol-utils.hh"

#include <cstdint>
#include <cstdio>
#include <string>

static int fail(const char *what)
{
    std::fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

int main()
{
    using namespace XemuLabelSymbolUtils;

    if (clean_c_symbol("__imp__helper@4") != "helper" ||
        clean_c_symbol("@Thunk@8") != "Thunk" ||
        clean_c_symbol("_g_Value") != "g_Value" ||
        clean_c_symbol("Plain") != "Plain") {
        return fail("C/stdcall/import cleanup");
    }

    if (lower_ascii("XAPILIB.LIB") != "xapilib.lib" ||
        upper_ascii("xboxkrnl") != "XBOXKRNL") {
        return fail("ASCII case normalization");
    }

    if (!compiler_internal_symbol("$SG123") ||
        !compiler_internal_symbol("_$E1") ||
        !compiler_internal_symbol("__safe_se_handler_foo") ||
        compiler_internal_symbol("PublicSymbol")) {
        return fail("common compiler-internal symbol filtering");
    }

    if (display_microsoft_symbol("?Pause@cBackgroundLoader@@UAEXXZ") !=
            "cBackgroundLoader::Pause" ||
        display_microsoft_symbol("__imp__helper@4") != "helper" ||
        !display_microsoft_symbol("$SG123").empty()) {
        return fail("common Microsoft display symbol cleanup");
    }

    if (simple_msvc_name("?Pause@cBackgroundLoader@@UAEXXZ") !=
            "cBackgroundLoader::Pause" ||
        simple_msvc_name("??0cBackgroundLoader@@QAE@H@Z") !=
            "cBackgroundLoader::cBackgroundLoader" ||
        simple_msvc_name("??1cBackgroundLoader@@QAE@XZ") !=
            "cBackgroundLoader::~cBackgroundLoader" ||
        simple_msvc_name("?Run@Inner@Outer@@SAXXZ") !=
            "Outer::Inner::Run") {
        return fail("simple MSVC name cleanup");
    }

    if (!simple_msvc_name("??2@YAPAXI@Z").empty() ||
        !simple_msvc_name("?Func@?$Thing@H@@SAXXZ").empty() ||
        !simple_msvc_name("not_msvc").empty()) {
        return fail("unsupported MSVC forms stay untouched by caller");
    }

    struct TestLabel {
        uint32_t virtual_address;
        int type;
        std::string name;
    };
    std::vector<TestLabel> labels = {
        {0x20u, 1, "B"}, {0x10u, 2, "C"}, {0x10u, 1, "B"},
        {0x10u, 1, "A"}, {0x10u, 1, "A"},
    };
    sort_and_dedupe_labels(labels);
    if (labels.size() != 4 ||
        labels[0].virtual_address != 0x10u || labels[0].type != 1 || labels[0].name != "A" ||
        labels[1].virtual_address != 0x10u || labels[1].type != 1 || labels[1].name != "B" ||
        labels[2].virtual_address != 0x10u || labels[2].type != 2 || labels[2].name != "C" ||
        labels[3].virtual_address != 0x20u || labels[3].type != 1 || labels[3].name != "B") {
        return fail("shared label ordering/deduplication");
    }

    std::puts("PASS: shared Microsoft symbol-display helper cases");
    return 0;
}
