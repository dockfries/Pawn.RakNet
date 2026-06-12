/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2023 katursis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PAWNRAKNET_PLUGIN_H_
#define PAWNRAKNET_PLUGIN_H_

class Plugin {
 public:
  static Plugin &Instance();
  static Plugin &Get() { return Instance(); }

  static bool Load(IPawnComponent *pawn_component, ICore *core);
  static void Unload();
  static void AddScript(IPawnScript *pawn_script);
  static void RemoveScript(IPawnScript *pawn_script);
  static void ProcessTick();

  static std::tuple<int, int, int> VersionToTuple(int version);

  template <typename... Args>
  static void Log(const std::string &fmt, Args... args) {
    Instance().LogImpl(fmt, args...);
  }

  const char *Name() { return "Pawn.RakNet"; }
  int Version() const { return PAWNRAKNET_VERSION; }
  std::string VersionAsString() const;

  bool OnLoad();
  bool LogAmxErrors() { return config_ && config_->LogAmxErrors(); }
  void OnUnload();
  void OnProcessTick();

  void SetCustomRPC(RPCIndex rpc_id);
  bool IsCustomRPC(RPCIndex rpc_id);

  const std::shared_ptr<Config> &GetConfig();

  BitStreamPool &GetPool() { return pool_; }

  ScriptData &GetScriptData(AMX *amx);

  template <PR_EventType event_type>
  static bool OnEvent(int player_id, unsigned char event_id, BitStream *bs) {
    return Instance().OnEventImpl<event_type>(player_id, event_id, bs);
  }

  template <typename... Args>
  void LogImpl(const std::string &fmt, Args... args) {
    if (core_) {
      core_->logLn(LogLevel::Message, ("[%s] " + fmt).c_str(), Name(),
                   args...);
    }
  }

 private:
  Plugin() = default;
  Plugin(const Plugin &) = delete;
  Plugin &operator=(const Plugin &) = delete;

  template <PR_EventType event_type>
  bool OnEventImpl(int player_id, unsigned char event_id, BitStream *bs) {
    for (auto *script : script_list_) {
      auto it = scripts_data_.find(script->GetAMX());
      if (it != scripts_data_.end()) {
        if (!it->second.OnEvent<event_type>(player_id, event_id, bs)) {
          return false;
        }
      }
    }
    return true;
  }

  ICore *core_{};
  IPawnComponent *pawn_component_{};

  std::shared_ptr<Config> config_;
  std::array<bool, PR_MAX_HANDLERS> custom_rpc_{};

  BitStreamPool pool_;

  std::unordered_map<AMX *, ScriptData> scripts_data_;
  std::vector<IPawnScript *> script_list_;
};

#endif  // PAWNRAKNET_PLUGIN_H_
