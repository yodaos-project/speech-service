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

static bool parse_cmdline(shared_ptr<CLArgs>& h, CmdlineArgs& res) {
  int32_t iv;
  uint32_t sz = h->size();
  uint32_t i;
  CLPair pair;

  for (i = 1; i < sz; ++i) {
    h->at(i, pair);
		if (pair.match("flora-bufsize")) {
      if (!pair.to_integer(iv))
				goto invalid_option;
			res.flora_bufsize = iv;
		} else if (pair.match("flora-uri")) {
			if (pair.value == nullptr || pair.value[0] == '\0')
				goto invalid_option;
			res.flora_uri = pair.value;
			res.flora_uri.append("#");
			res.flora_uri.append(TAG);
		} else if (pair.match("flora-reconn-interval")) {
      if (!pair.to_integer(iv))
				goto invalid_option;
			res.flora_reconn_interval = milliseconds(iv);
		} else if (pair.match("log-service-port")) {
      if (!pair.to_integer(iv))
				goto invalid_option;
			res.log_service_port = iv;
		} else if (pair.match("lastest-speech-file")) {
			if (pair.value == nullptr || pair.value[0] == '\0')
				goto invalid_option;
			res.lastest_speech_file = pair.value;
		} else if (pair.match("skillopt-pro")) {
			if (pair.value == nullptr || pair.value[0] == '\0')
				goto invalid_option;
			res.skilloptions_provider = pair.value;
		} else
			goto invalid_option;
	}
	return true;

invalid_option:
	if (pair.value)
		KLOGE(TAG, "invalid option: --%s=%s", pair.key, pair.value);
	else
		KLOGE(TAG, "invalid option: --%s", pair.key);
	return false;
}

int main(int argc, char** argv) {
  shared_ptr<CLArgs> h = CLArgs::parse(argc, argv);
	if (h == nullptr || h->find("help", nullptr, nullptr)) {
		print_prompt(argv[0]);
		return 0;
	}
	if (h->find("version", nullptr, nullptr)) {
		KLOGI(TAG, "git commit id: %s", MACRO_TO_STRING(GIT_COMMIT_ID));
		return 0;
	}
	CmdlineArgs cmdargs;
	if (!parse_cmdline(h, cmdargs)) {
		return 1;
	}
  h.reset();

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
