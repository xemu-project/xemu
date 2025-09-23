#include "github-issue.hh"

#include <string>

#include "common.hh"
#include <curl/curl.h>
#include <glib/gunicode.h>
#include "hw/xbox/nv2a/nv2a.h"
#include "qemu/http.h"
#include "ui/xemu-os-utils.h"
#include "ui/xemu-settings.h"
#include "xemu-version.h"
#include "xemu-xbe.h"

static constexpr char base_compatibility_url[] = "https://xemu.app/titles/";
static constexpr char base_issue_url[] =
    "https://github.com/xemu-project/xemu/issues/new?template=";
static constexpr char title_issue_template[] = "title-issue.yml";

static std::string BuildTitleInformation(struct xbe *xbe)
{
    std::string ret = base_compatibility_url;

    char title_id_buffer[32];
    snprintf(title_id_buffer, sizeof(title_id_buffer), "%x",
             xbe->cert->m_titleid);
    ret += title_id_buffer;

    ret += "/";

    char *xbe_title_name = g_utf16_to_utf8(xbe->cert->m_title_name,
                                           std::size(xbe->cert->m_title_name),
                                           nullptr, nullptr, nullptr);
    if (xbe_title_name) {
        ret += "#";
        ret += xbe_title_name;
        g_free(xbe_title_name);
    }
    ret += "\n";

    return ret;
}

static std::string BuildXemuInformation()
{
    std::string ret = "* Version: ";
    ret += xemu_version;
    ret += "\n* Commit: ";
    ret += xemu_commit;
    ret += "\n* Date: ";
    ret += xemu_date;
    ret += "\n\n";
    return ret;
}

static std::string BuildSystemInformation()
{
    auto safe_gl_str = [](GLenum name) {
        const GLubyte *s = glGetString(name);
        return s ? (const char *)s : "<<Unknown>>";
    };

    std::string ret = "OS Info:\n* Platform: ";
    ret += SDL_GetPlatform();
    ret += "\n* Version: ";
    ret += xemu_get_os_info();
    ret += "\n";

    ret += "CPU: ";
    ret += xemu_get_cpu_info();
    ret += "\n";

    ret += "GPU Info:\n* Vendor: ";
    ret += safe_gl_str(GL_VENDOR);
    ret += "\n* Renderer: ";
    ret += safe_gl_str(GL_RENDERER);
    ret += "\n* GL Version: ";
    ret += safe_gl_str(GL_VERSION);
    ret += "\n* GLSL Version: ";
    ret += safe_gl_str(GL_SHADING_LANGUAGE_VERSION);
    ret += "\n";

    return ret;
}

static std::string BuildAdditionalInformation()
{
    char buf[64];
    snprintf(buf, sizeof(buf), "* Resolution scale: %dx\n",
             nv2a_get_surface_scale_factor());

    std::string ret = buf;

    ret += "* Renderer backend: ";
    switch (g_config.display.renderer) {
    case CONFIG_DISPLAY_RENDERER_NULL:
        ret += "NULL";
        break;
    case CONFIG_DISPLAY_RENDERER_OPENGL:
        ret += "OpenGL";
        break;
    case CONFIG_DISPLAY_RENDERER_VULKAN:
        ret += "Vulkan";
        break;
    default:
        ret += "UNKNOWN - update ";
        ret += __FILE__;
        break;
    }
    ret += "\n";

    ret += "* Realtime DSP: ";
    ret += g_config.audio.use_dsp ? "ON" : "OFF";
    ret += "\n";

    ret += "* System memory: ";
    switch (g_config.sys.mem_limit) {
    case CONFIG_SYS_MEM_LIMIT_64:
        ret += "64MiB";
        break;
    case CONFIG_SYS_MEM_LIMIT_128:
        ret += "128MiB";
        break;
    default:
        ret += "UNKNOWN - update ";
        ret += __FILE__;
        break;
    }
    ret += "\n";

    ret += "* AV pack: ";
    switch (g_config.sys.avpack) {
    case CONFIG_SYS_AVPACK_SCART:
        ret += "SCART";
        break;
    case CONFIG_SYS_AVPACK_HDTV:
        ret += "HDTV";
        break;
    case CONFIG_SYS_AVPACK_VGA:
        ret += "VGA";
        break;
    case CONFIG_SYS_AVPACK_RFU:
        ret += "RFU";
        break;
    case CONFIG_SYS_AVPACK_SVIDEO:
        ret += "SVIDEO";
        break;
    case CONFIG_SYS_AVPACK_COMPOSITE:
        ret += "Composite";
        break;
    case CONFIG_SYS_AVPACK_NONE:
        ret += "None";
        break;
    default:
        ret += "UNKNOWN - update ";
        ret += __FILE__;
        break;
    }

    return ret;
}

static std::string BuildGitHubTitleIssueURL(struct xbe *xbe)
{
    std::string ret = base_issue_url;
    ret += title_issue_template;

    if (!ensure_libcurl_initialized(nullptr)) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return ret;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return ret;
    }

    auto append_param = [&curl, &ret](const char *name,
                                      const std::string &value) {
        ret += name;
        char *escaped = curl_easy_escape(curl, value.c_str(), value.length());
        if (escaped) {
            ret += escaped;
            curl_free(escaped);
        }
    };

    append_param("&game-title=", BuildTitleInformation(xbe));
    append_param("&xemu-version=", BuildXemuInformation());
    append_param("&system-information=", BuildSystemInformation());
    append_param("&additional-context=", BuildAdditionalInformation());

    curl_easy_cleanup(curl);

    return ret;
}

void ShowReportGitHubIssueMenuItem()
{
    struct xbe *xbe = xemu_get_xbe_info();
    if (!xbe) {
        return;
    }

    if (ImGui::MenuItem("Report GitHub Title Issue...", NULL)) {
        SDL_OpenURL(BuildGitHubTitleIssueURL(xbe).c_str());
    }
}
