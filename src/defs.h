#pragma once

#include <chrono>
#include <stdint.h>
#include <string>

#define TAG "speech-service"

class CmdlineArgs {
public:
  CmdlineArgs() : flora_reconn_interval(5000) {}

  uint32_t flora_bufsize = 0;
  std::string flora_uri = "unix:flora-dispatcher-socket#" TAG;
  std::chrono::milliseconds flora_reconn_interval;
  int32_t log_service_port = 0;
  std::string lastest_speech_file;
  std::string skilloptions_provider = "vui";
};

#define SPEECH_SERVICE_RPC_ERROR_NOT_READY -1
#define SPEECH_SERVICE_RPC_ERROR_INVALID_PARAM -2
#define SPEECH_SERVICE_RPC_ERROR_CLOUD -3
