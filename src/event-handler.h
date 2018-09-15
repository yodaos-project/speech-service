#pragma once

#include <mutex>
#include <condition_variable>
#include <map>
#include <list>
#include <utility>
#include "defs.h"
#include "speech.h"
#include "flora-cli.h"

class EventHandler;
typedef void (EventHandler::*EventHandleFunc)(uint32_t msgtype, std::shared_ptr<Caps>& msg);
typedef std::map<std::string, EventHandleFunc> EventHandlerMap;
typedef struct {
  std::string msgid;
  int32_t speechid;
  int32_t custom;
} TextReqInfo;

class EventHandler : public flora::ClientCallback {
public:
  void init(CmdlineArgs& args);

  void set_flora_client(std::shared_ptr<flora::Client>& cli);


  // callbacks of flora::ClientCallback
  void recv_post(const char* name, uint32_t msgtype,
      std::shared_ptr<Caps>& msg);

  void disconnected();

public:
  void handle_speech_prepare_options(uint32_t msgtype, std::shared_ptr<Caps>& msg);
  void handle_speech_options(uint32_t msgtype, std::shared_ptr<Caps>& msg);
  void handle_speech_stack(uint32_t msgtype, std::shared_ptr<Caps>& msg);
  void handle_turen_start_voice(uint32_t msgtype, std::shared_ptr<Caps>& msg);
  void handle_turen_voice(uint32_t msgtype, std::shared_ptr<Caps>& msg);
  void handle_turen_sleep(uint32_t msgtype, std::shared_ptr<Caps>& msg);
  void handle_speech_put_text(uint32_t msgtype, std::shared_ptr<Caps>& msg);

private:
  void do_speech_poll();
  void flora_disconnected();
  void post_error(int32_t err, int32_t id, int32_t turen_id);
  void post_error(int32_t err, const std::string& extid,
      int32_t custom, int32_t turen_id, std::shared_ptr<flora::Client>& cli);
  bool check_pending_texts(int32_t id, std::string& extid, int32_t& custom);
  void finish_voice_req(int32_t speech_id);

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
  bool speech_prepared = false;
};
