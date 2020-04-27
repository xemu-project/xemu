#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include "xemu-reporting.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#include "httplib.h"
#include "json.hpp"
using json = nlohmann::json;

CompatibilityReport::CompatibilityReport()
{
}

CompatibilityReport::~CompatibilityReport()
{
}

const std::string &CompatibilityReport::GetSerializedReport()
{
	json report = {
		{"token", token},
		{"xemu_version", xemu_version},
		{"xemu_branch", xemu_branch},
		{"xemu_commit", xemu_commit},
		{"xemu_date", xemu_date},
		{"os_platform", os_platform},
		{"os_version", os_version},
		{"cpu", cpu},
		{"gl_vendor", gl_vendor},
		{"gl_renderer", gl_renderer},
		{"gl_version", gl_version},
		{"gl_shading_language_version", gl_shading_language_version},
		{"compat_rating", compat_rating},
		{"compat_comments", compat_comments},
		{"xbe_headers", xbe_headers},
	};
	serialized = report.dump(2); 
	return serialized;
}

bool CompatibilityReport::Send()
{
	// Serialize the report
	const std::string &s = GetSerializedReport();

	httplib::SSLClient cli("127.0.0.1", 443);
	// httplib::SSLClient cli("reports.xemu.app", 443);

	cli.set_follow_location(true);
	cli.set_timeout_sec(5);
	// cli.enable_server_certificate_verification(true); // FIXME: Package cert bundle

	auto res = cli.Post("/compatibility", s, "application/json");

	if (!res) {
#if 0 // FIXME: Handle SSL certificate verification failure
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
	    auto result = cli.get_openssl_verify_result();
	    if (result) {
	      fprintf(stderr, "verify error: %s\n", X509_verify_cert_error_string(result));
	    }
#endif
#endif

		result_code = -1;
		result_msg = "Failed to connect";
	    return false;
	}

	result_code = res->status;

	switch(res->status) {
	case 200:
		result_msg = "Ok";
		return true;

	case 400:
	case 411:
		result_msg = "Invalid request";
		return false;

	case 403:
		result_msg = "Invalid token";
		return false;

	case 413:
		result_msg = "Report too long";
		return false;

	default:
		result_msg = "Unknown error occured";
		return false;
	}
}

void CompatibilityReport::SetXbeData(struct xbe *xbe)
{
	assert(xbe != NULL);
	assert(xbe->headers != NULL);
	assert(xbe->headers_len > 0);

    // base64 encode all XBE headers to be sent with the report
    gchar *buf = g_base64_encode(xbe->headers, xbe->headers_len);
    xbe_headers = buf;
    g_free(buf);
}
