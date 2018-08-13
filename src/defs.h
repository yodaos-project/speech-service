#pragma once

#include <stdint.h>
#include <string>
#include <chrono>

#define TAG "speech-service"

class CmdlineArgs {
public:
	CmdlineArgs() : flora_reconn_interval(5000) {
	}

	uint32_t flora_bufsize = 0;
	std::string flora_uri = "tcp://0.0.0.0:2517/";
	std::chrono::milliseconds flora_reconn_interval;
};
