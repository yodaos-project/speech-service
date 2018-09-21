#pragma once

#include <mutex>
#include <condition_variable>
#include <map>
#include <list>
#include <utility>
#include "defs.h"
#include "speech.h"
#include "flora-cli.h"

class FloraMsgInfo {
public:
  std::string name;
  uint32_t type;
};
class MsgCompare {
public:
  bool operator()(const FloraMsgInfo& lhs, const FloraMsgInfo& rhs) const {
    if (lhs.name < rhs.name)
      return true;
    if (lhs.name == rhs.name)
      return lhs.type < rhs.type;
    return false;
  }
};
typedef std::map<FloraMsgInfo,
        std::function<void(std::shared_ptr<Caps>&)>,
        MsgCompare> EventHandlerMap;

typedef struct {
  std::string msgid;
  int32_t speechid;
  int32_t custom;
} TextReqInfo;

class EventHandler : public flora::ClientCallback {
public:
  void init(CmdlineArgs& args);


  // callbacks of flora::ClientCallback
  void recv_post(const char* name, uint32_t msgtype,
      std::shared_ptr<Caps>& msg);

  void disconnected();

private:
  void do_speech_poll();
  void flora_keepalive(CmdlineArgs& args);
  void subscribe_events(std::shared_ptr<flora::Client>& cli);
  void flora_disconnected();
  bool check_pending_texts(int32_t id, std::string& extid, int32_t& custom);
  void finish_voice_req(int32_t speech_id);
  void post_completed(int32_t turen_id);
  void post_error(const char* suffix, int32_t err, int32_t id);
  void post_error(int32_t err, int32_t speech_id);
  void post_inter_asr(const std::string& asr, int32_t speech_id);
  void post_extra(const std::string& extra, int32_t speech_id);
  void post_final_asr(const std::string& asr, int32_t speech_id);
  void post_nlp(const char* suffix, const std::string& nlp,
      const std::string& action, int32_t id);
  void post_nlp(const std::string& nlp, const std::string& action,
      int32_t speech_id);
  int32_t get_turen_id(int32_t speech_id);
  int32_t get_speech_id(int32_t turen_id);

  // flora msg handlers
  void handle_speech_prepare_options(std::shared_ptr<Caps>& msg);
  void handle_speech_options(std::shared_ptr<Caps>& msg);
  void handle_speech_stack(std::shared_ptr<Caps>& msg);
  void handle_turen_start_voice(std::shared_ptr<Caps>& msg);
  void handle_turen_voice(std::shared_ptr<Caps>& msg);
  void handle_turen_end_voice(std::shared_ptr<Caps>& msg);
  void handle_turen_sleep(std::shared_ptr<Caps>& msg);
  void handle_speech_put_text(std::shared_ptr<Caps>& msg);
public:
  std::mutex reconn_mutex;
  std::condition_variable reconn_cond;
  std::shared_ptr<rokid::speech::Speech> speech;
  std::shared_ptr<flora::Client> flora_cli;
  EventHandlerMap handlers;
  std::string speech_stack;
  std::mutex speech_mutex;
  std::condition_variable speech_cond;
  // mutex for speech voice request
  std::mutex voice_mutex;
  // not finish voice request
  // turen id --- speech id
  std::list<std::pair<int32_t, int32_t> > pending_voices;
  // mutex for speech put_text and poll
  std::mutex text_mutex;
  std::list<TextReqInfo> pending_texts;
  std::mutex completed_mutex;
  int32_t lastest_completed_id = 0;
  std::string lastest_speech_file;
  bool speech_prepared = false;
};
