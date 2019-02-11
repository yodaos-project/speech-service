#pragma once

#include "defs.h"
#include "flora-agent.h"
#include "speech.h"
#include <list>

typedef struct {
  int32_t speechid;
  std::shared_ptr<flora::Reply> reply;
} TextReqInfo;

class SpeechService {
public:
  SpeechService();

  void run(CmdlineArgs &args);

private:
  // flora msg handlers
  void handle_speech_prepare_options(std::shared_ptr<Caps> &msg);
  void handle_speech_options(std::shared_ptr<Caps> &msg);
  void handle_speech_stack(std::shared_ptr<Caps> &msg);
  void handle_turen_start_voice(std::shared_ptr<Caps> &msg);
  void handle_turen_voice(std::shared_ptr<Caps> &msg);
  void handle_turen_end_voice(std::shared_ptr<Caps> &msg);
  void handle_turen_sleep(std::shared_ptr<Caps> &msg);
  void handle_asr2nlp(std::shared_ptr<Caps> &msg,
                      std::shared_ptr<flora::Reply> &reply);

private:
  bool init_speech(std::shared_ptr<Caps> &data);

  void reconn_speech(std::shared_ptr<Caps> &data);

  bool config_speech(std::shared_ptr<Caps> &data);

  void do_speech_poll();

  void post_nlp(const std::string &nlp, const std::string &action, int32_t id);

  void post_error(int32_t err, int32_t id);

  void post_inter_asr(const std::string &asr, int32_t turen_id);

  void post_extra(const std::string &extra, int32_t turen_id);

  void post_final_asr(const std::string &asr, int32_t turen_id);

  void post_completed(int32_t turen_id);

  void finish_voice_req(int32_t speech_id);

  int32_t get_turen_id(int32_t speech_id);

  int32_t get_speech_id(int32_t turen_id);

  bool end_pending_texts(int32_t speech_id, const std::string &nlp,
                         const std::string &action);

  bool end_pending_texts(int32_t speech_id, int32_t err);

  bool pop_text_req(int32_t speech_id, TextReqInfo &res);

  void get_skilloptions(std::string &res);

private:
  CmdlineArgs *cmdline_args = nullptr;
  std::shared_ptr<rokid::speech::Speech> speech;
  std::string stack;
  flora::Agent flora_agent;
  flora::Agent skilloptions_agent;
  // mutex for speech voice request
  std::mutex voice_mutex;
  // not finish voice request
  // turen id --- speech id
  std::list<std::pair<int32_t, int32_t>> pending_voices;
  // mutex for speech put_text and poll
  std::mutex text_mutex;
  std::list<TextReqInfo> pending_texts;
  std::mutex completed_mutex;
  int32_t lastest_completed_id = 0;
  std::string lastest_speech_file;
  bool first_prepare = true;
};
