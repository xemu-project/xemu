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
		{"memory", memory},
		{"xbe_timestamp", xbe_timestamp},
		{"xbe_cert_timestamp", xbe_cert_timestamp},
		{"xbe_cert_title_id", xbe_cert_title_id},
		{"xbe_cert_region", xbe_cert_region},
		{"xbe_cert_disc_num", xbe_cert_disc_num},
		{"xbe_cert_version", xbe_cert_version},
		{"compat_rating", compat_rating},
		{"compat_comments", compat_comments},
	};
	serialized = report.dump(2); 
	return serialized;
}

void CompatibilityReport::Send()
{
	// Serialize the report
	const std::string &s = GetSerializedReport();

	fprintf(stderr, "%s\n", s.c_str());

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
	    return;
	}

	fprintf(stderr, "%d\n", res->status);
}
