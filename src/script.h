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

#ifndef PAWNRAKNET_SCRIPT_DATA_H_
#define PAWNRAKNET_SCRIPT_DATA_H_

class Public {
 public:
  Public(const std::string &name, AMX *amx, bool use_caching = false);

  template <typename... Args>
  inline cell Exec(Args... args) {
    cell retval{};

    if (use_caching_) {
      if (!cached_) {
        amx_FindPublic(amx_, name_.c_str(), &index_);
        cached_ = true;
      }
    } else {
      amx_FindPublic(amx_, name_.c_str(), &index_);
    }

    if constexpr (sizeof...(Args) != 0) {
      Push(args...);
    }

    amx_Exec(amx_, &retval, index_);

    if (amx_addr_to_release_) {
      amx_Release(amx_, amx_addr_to_release_);
      amx_addr_to_release_ = 0;
    }

    return retval;
  }

  // Query the public's existence live. Do not cache the result at
  // construction time: sampgdk's FindPublic hook (which forges a negative
  // index for callbacks that only exist in C++) may not be installed yet
  // when the Public is constructed, so a cached bool would be stale.
  bool Exists() {
    return amx_FindPublic(amx_, name_.c_str(), &index_) == AMX_ERR_NONE;
  }

 private:
  template <typename T, typename... Args>
  void Push(T arg1, Args... args) {
    Push(args...);
    Push(arg1);
  }

  template <typename T>
  void Push(T arg) {
    if constexpr (std::is_pointer<T>::value) {
      if constexpr (std::is_same<T, const char *>::value ||
                    std::is_same<T, char *>::value) {
        cell amx_addr{};
        amx_PushString(amx_, &amx_addr, nullptr, arg, false, false);
        if (!amx_addr_to_release_) {
          amx_addr_to_release_ = amx_addr;
        }
      } else {
        amx_Push(amx_, reinterpret_cast<cell>(arg));
      }
    } else if constexpr (std::is_floating_point<T>::value) {
      amx_Push(amx_, amx_ftoc(arg));
    } else if constexpr (std::is_same<typename std::decay<T>::type,
                                      std::string>::value) {
      Push(arg.c_str());
    } else {
      amx_Push(amx_, static_cast<cell>(arg));
    }
  }

  AMX *amx_{};
  std::string name_;
  int index_{};
  bool cached_{};
  bool use_caching_{};
  cell amx_addr_to_release_{};
};

using PublicPtr = std::shared_ptr<Public>;

struct ScriptData {
  AMX *amx_{};

  void SetAMX(AMX *amx) { amx_ = amx; }

  void OnLoad();
  bool ExecPublic(const PublicPtr &pub, int player_id, unsigned char event_id,
                  BitStream *bs);
  bool CallbackExec(const PublicPtr &pub, int player_id, BitStream *bs);

  void InitPublic(PR_EventType type, const std::string &public_name);
  void InitHandler(unsigned char event_id, const std::string &public_name,
                   PR_EventType type);
  void InitHandlers();
  cell BS_WriteValue(cell *params);
  cell BS_ReadValue(cell *params);

  template <PR_EventType event_type>
  bool OnEvent(int player_id, unsigned char event_id, BitStream *bs) {
    if constexpr (event_type == PR_OUTGOING_PACKET) {
      if (!ExecPublic(public_on_outcoming_packet_, player_id, event_id, bs)) {
        return false;
      }
    } else if constexpr (event_type == PR_OUTGOING_RPC) {
      if (!ExecPublic(public_on_outcoming_rpc_, player_id, event_id, bs)) {
        return false;
      }
    }

    if constexpr (event_type != PR_INCOMING_CUSTOM_RPC) {
      if (!ExecPublic(std::get<event_type>(publics_), player_id, event_id,
                      bs)) {
        return false;
      }
    }

    for (const auto &handler : std::get<event_type>(handlers_).at(event_id)) {
      bs->resetReadPointer();
      if (!CallbackExec(handler, player_id, bs)) {
        return false;
      }
    }

    bs->resetReadPointer();
    return true;
  }

 private:
  template <typename T, bool compressed = false>
  void WriteValue(BitStream *bs, cell value);
  template <typename T, bool compressed = false>
  cell ReadValue(BitStream *bs);

  cell *GetPhysAddr(cell amx_addr);
  std::string GetString(cell amx_addr);
  void SetString(cell *dest, const std::string &src, std::size_t size);
  std::string GetPublicName(int index);
  std::shared_ptr<Public> MakePublic(const std::string &name,
                                     bool use_caching = false);
  void AssertMinParams(std::size_t min_count, cell *params) const;

  const std::regex regex_reg_handler_public_name_{
      R"(^pr_r(?:ip|ir|op|or|irp|iip|oip|icr)_\w+$)"};

  std::shared_ptr<Config> config_;

  std::list<PublicPtr> publics_reg_handler_;
  std::array<PublicPtr, PR_NUMBER_OF_EVENT_TYPES> publics_;
  std::array<std::array<std::list<PublicPtr>, PR_MAX_HANDLERS>,
             PR_NUMBER_OF_EVENT_TYPES>
      handlers_;
  PublicPtr public_on_outcoming_packet_;
  PublicPtr public_on_outcoming_rpc_;
};

#endif  // PAWNRAKNET_SCRIPT_DATA_H_
