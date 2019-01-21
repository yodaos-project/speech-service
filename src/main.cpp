#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "flora-svc.h"
#include "clargs.h"
#include "rlog.h"
#include "defs.h"
#include "event-handler.h"

#define MACRO_TO_STRING(x) MACRO_TO_STRING1(x)
#define MACRO_TO_STRING1(x) #x

using namespace flora;
using namespace std;
using namespace std::chrono;

static void print_prompt(const char* progname) {
	static const char* prompt =
		"USAGE: %s [options]\n"
		"options:\n"
		"\t--help    打印帮助信息\n"
		"\t--version    版本号\n"
		"\t--flora-uri=*    flora服务uri\n"
		"\t--flora-bufsize=*    flora消息缓冲区大小\n"
		"\t--flora-reconn-interval=*    flora服务重连时间间隔(ms)\n"
		"\t--log-service-port=*    log服务端口\n"
		"\t--lastest-speech-file=*    保存最近一次speech语音数据至指定文件\n"
		"\t--skillopt-pro=*    skilloptions provider的flora名称\n"
		;
	KLOGI(TAG, prompt, progname);
}

void run(CmdlineArgs& args);

static bool parse_cmdline(clargs_h h, CmdlineArgs& res) {
	const char* key;
	const char* val;
	char* ep;
	long iv;

	while (clargs_opt_next(h, &key, &val) == 0) {
		if (strcmp(key, "flora-bufsize") == 0) {
			if (val[0] == '\0')
				goto invalid_option;
			iv = strtol(val, &ep, 10);
			if (ep[0] != '\0')
				goto invalid_option;
			res.flora_bufsize = iv;
		} else if (strcmp(key, "flora-uri") == 0) {
			if (val[0] == '\0')
				goto invalid_option;
			res.flora_uri = val;
			res.flora_uri.append("#");
			res.flora_uri.append(TAG);
		} else if (strcmp(key, "flora-reconn-interval") == 0) {
			if (val[0] == '\0')
				goto invalid_option;
			iv = strtol(val, &ep, 10);
			if (ep[0] != '\0')
				goto invalid_option;
			res.flora_reconn_interval = milliseconds(iv);
		} else if (strcmp(key, "log-service-port") == 0) {
			if (val[0] == '\0')
				goto invalid_option;
			iv = strtol(val, &ep, 10);
			if (ep[0] != '\0')
				goto invalid_option;
			res.log_service_port = iv;
		} else if (strcmp(key, "lastest-speech-file") == 0) {
			if (val[0] == '\0')
				goto invalid_option;
			res.lastest_speech_file = val;
		} else if (strcmp(key, "skillopt-pro") == 0) {
			if (val[0] == '\0')
				goto invalid_option;
			res.skilloptions_provider = val;
		} else
			goto invalid_option;
	}
	return true;

invalid_option:
	if (val[0])
		KLOGE(TAG, "invalid option: --%s=%s", key, val);
	else
		KLOGE(TAG, "invalid option: --%s", key);
	return false;
}

int main(int argc, char** argv) {
	clargs_h h = clargs_parse(argc, argv);
	if (!h || clargs_opt_has(h, "help")) {
		clargs_destroy(h);
		print_prompt(argv[0]);
		return 0;
	}
	if (clargs_opt_has(h, "version")) {
		clargs_destroy(h);
		KLOGI(TAG, "git commit id: %s", MACRO_TO_STRING(GIT_COMMIT_ID));
		return 0;
	}
	CmdlineArgs cmdargs;
	if (!parse_cmdline(h, cmdargs)) {
		clargs_destroy(h);
		return 1;
	}
	clargs_destroy(h);

	run(cmdargs);
	return 0;
}

void run(CmdlineArgs& args) {
  if (args.log_service_port > 0) {
    TCPSocketArg rlogarg;
    rlogarg.host = "0.0.0.0";
    rlogarg.port = args.log_service_port;
    rokid_log_ctl(ROKID_LOG_CTL_DEFAULT_ENDPOINT, "tcp-socket", &rlogarg);
  }

	EventHandler event_handler;
	event_handler.init(args);
}
