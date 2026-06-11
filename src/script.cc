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

#include "main.h"

Public::Public(const std::string &name, IPawnScript *script, bool use_caching)
    : script_{script},
      name_{name},
      use_caching_{use_caching} {
  exists_ = script_->FindPublic(name_.c_str(), &index_) == AMX_ERR_NONE &&
            index_ >= 0;
}

void Script::Init(IPawnScript *pawn_script) {
  pawn_script_ = pawn_script;
}

AMX *Script::GetAmx() const {
  return pawn_script_->GetAMX();
}

cell *Script::GetPhysAddr(cell amx_addr) {
  cell *phys_addr{};
  pawn_script_->GetAddr(amx_addr, &phys_addr);
  return phys_addr;
}

std::string Script::GetString(cell amx_addr) {
  cell *addr = GetPhysAddr(amx_addr);

  int len{};
  pawn_script_->StrLen(addr, &len);

  std::unique_ptr<char[]> str{new char[len + 1]{}};
  pawn_script_->GetString(str.get(), addr, false, len + 1);

  return str.get();
}

void Script::SetString(cell *dest, const std::string &src, std::size_t size) {
  pawn_script_->SetString(dest, src, false, false, size);
}

std::string Script::GetPublicName(int index) {
  int len{};
  pawn_script_->NameLength(&len);

  std::unique_ptr<char[]> name{new char[len + 1]{}};
  pawn_script_->GetPublic(index, name.get());

  return name.get();
}

std::shared_ptr<Public> Script::MakePublic(const std::string &name,
                                           bool use_caching) {
  return std::make_shared<Public>(name, pawn_script_, use_caching);
}

void Script::AssertMinParams(std::size_t min_count, cell *params) const {
  if (static_cast<ucell>(params[0]) < (min_count * sizeof(cell))) {
    throw std::runtime_error{"Number of parameters must be >= " +
                             std::to_string(min_count)};
  }
}

cell Script::BS_New() { return bitstream_pool_.New(); }

cell Script::BS_NewCopy(BitStream *bs) {
  const auto bs_handle = bitstream_pool_.New();
  const auto bs_copy = bitstream_pool_.Get(bs_handle);

  int original_read_offset = bs->GetReadOffset();

  bs->resetReadPointer();

  bs_copy->Write(bs);

  bs->SetReadOffset(original_read_offset);

  return bs_handle;
}

cell Script::BS_Delete(cell *bs) {
  bitstream_pool_.Delete(*bs);
  *bs = 0;

  return 1;
}

void Script::PR_Init() {
  InitHandlers();
}

void Script::PR_RegHandler(unsigned char event_id,
                           const std::string &public_name, PR_EventType type) {
  InitHandler(event_id, public_name, type);
}

// BS_WriteValue and BS_ReadValue remain here because they're too complex to inline.
cell Script::BS_WriteValue(cell *params) {
  AssertMinParams(3, params);

  const auto bs = GetBitStream(params[1]);

  for (std::size_t i = 1; i < (params[0] / sizeof(cell)) - 1; i += 2) {
    const auto type = *GetPhysAddr(params[i + 1]);
    const auto &value = *GetPhysAddr(params[i + 2]);

    switch (type) {
      case PR_STRING:
      case PR_CSTRING: {
        auto str = GetString(params[i + 2]);

        if (type == PR_STRING) {
          bs->Write(str.c_str(), static_cast<int>(str.size()));
        } else {
          stringCompressor->EncodeString(str.c_str(), static_cast<int>(str.size() + 1), bs);
        }

        break;
      }
      case PR_INT8:
        WriteValue<char>(bs, value);
        break;
      case PR_INT16:
        WriteValue<short>(bs, value);
        break;
      case PR_INT32:
        WriteValue<int>(bs, value);
        break;
      case PR_UINT8:
        WriteValue<unsigned char>(bs, value);
        break;
      case PR_UINT16:
        WriteValue<unsigned short>(bs, value);
        break;
      case PR_UINT32:
        WriteValue<unsigned int>(bs, value);
        break;
      case PR_FLOAT:
        WriteValue<float>(bs, value);
        break;
      case PR_BOOL:
        WriteValue<bool>(bs, value);
        break;
      case PR_CINT8:
        WriteValue<char, true>(bs, value);
        break;
      case PR_CINT16:
        WriteValue<short, true>(bs, value);
        break;
      case PR_CINT32:
        WriteValue<int, true>(bs, value);
        break;
      case PR_CUINT8:
        WriteValue<unsigned char, true>(bs, value);
        break;
      case PR_CUINT16:
        WriteValue<unsigned short, true>(bs, value);
        break;
      case PR_CUINT32:
        WriteValue<unsigned int, true>(bs, value);
        break;
      case PR_CFLOAT:
        WriteValue<float, true>(bs, value);
        break;
      case PR_CBOOL:
        WriteValue<bool, true>(bs, value);
        break;
      case PR_BITS: {
        const auto number_of_bits = *GetPhysAddr(params[i + 3]);
        if (number_of_bits <= 0 || number_of_bits > (sizeof(cell) * 8)) {
          throw std::runtime_error{"Invalid number of bits"};
        }

        bs->WriteBits(reinterpret_cast<const unsigned char *>(&value),
                      number_of_bits, true);

        i++;

        break;
      }
      case PR_FLOAT3:
      case PR_FLOAT4: {
        const std::size_t arr_size = (type == PR_FLOAT3 ? 3 : 4);
        const auto arr = &value;

        for (std::size_t index{}; index < arr_size; index++) {
          WriteValue<float>(bs, arr[index]);
        }

        break;
      }
      case PR_VECTOR:
      case PR_NORM_QUAT: {
        const auto arr = reinterpret_cast<const float *>(&value);

        if (type == PR_VECTOR) {
          bs->WriteVector(arr[0], arr[1], arr[2]);
        } else {
          bs->WriteNormQuat(arr[0], arr[1], arr[2], arr[3]);
        }

        break;
      }
      case PR_STRING8:
      case PR_STRING32: {
        auto str = GetString(params[i + 2]);

        if (type == PR_STRING8) {
          WriteValue<unsigned char>(bs, static_cast<cell>(str.size()));
        } else {
          WriteValue<unsigned int>(bs, static_cast<cell>(str.size()));
        }

        bs->Write(str.c_str(), static_cast<int>(str.size()));

        break;
      }
      case PR_IGNORE_BITS: {
        bs->SetWriteOffset(bs->GetWriteOffset() + value);
        break;
      }
      default: {
        throw std::runtime_error{"Invalid type of value"};
      }
    }
  }

  return 1;
}

cell Script::BS_ReadValue(cell *params) {
  AssertMinParams(3, params);

  const auto bs = GetBitStream(params[1]);

  for (std::size_t i = 1; i < (params[0] / sizeof(cell)) - 1; i += 2) {
    const auto type = *GetPhysAddr(params[i + 1]);
    auto &value = *GetPhysAddr(params[i + 2]);

    switch (type) {
      case PR_STRING:
      case PR_CSTRING: {
        const auto size = *GetPhysAddr(params[i + 3]);

        std::unique_ptr<char[]> str{new char[size + 1]{}};

        if (type == PR_STRING) {
          bs->Read(str.get(), size);
        } else {
          stringCompressor->DecodeString(str.get(), size, bs);
        }

        SetString(&value, str.get(), size + 1);

        i++;

        break;
      }
      case PR_INT8:
        value = ReadValue<char>(bs);
        break;
      case PR_INT16:
        value = ReadValue<short>(bs);
        break;
      case PR_INT32:
        value = ReadValue<int>(bs);
        break;
      case PR_UINT8:
        value = ReadValue<unsigned char>(bs);
        break;
      case PR_UINT16:
        value = ReadValue<unsigned short>(bs);
        break;
      case PR_UINT32:
        value = ReadValue<unsigned int>(bs);
        break;
      case PR_FLOAT:
        value = ReadValue<float>(bs);
        break;
      case PR_BOOL:
        value = ReadValue<bool>(bs);
        break;
      case PR_CINT8:
        value = ReadValue<char, true>(bs);
        break;
      case PR_CINT16:
        value = ReadValue<short, true>(bs);
        break;
      case PR_CINT32:
        value = ReadValue<int, true>(bs);
        break;
      case PR_CUINT8:
        value = ReadValue<unsigned char, true>(bs);
        break;
      case PR_CUINT16:
        value = ReadValue<unsigned short, true>(bs);
        break;
      case PR_CUINT32:
        value = ReadValue<unsigned int, true>(bs);
        break;
      case PR_CFLOAT:
        value = ReadValue<float, true>(bs);
        break;
      case PR_CBOOL:
        value = ReadValue<bool, true>(bs);
        break;
      case PR_BITS: {
        const auto number_of_bits = *GetPhysAddr(params[i + 3]);
        if (number_of_bits <= 0 || number_of_bits > (sizeof(cell) * 8)) {
          throw std::runtime_error{"Invalid number of bits"};
        }

        bs->ReadBits(reinterpret_cast<unsigned char *>(&value), number_of_bits,
                     true);

        i++;

        break;
      }
      case PR_FLOAT3:
      case PR_FLOAT4: {
        const std::size_t arr_size = (type == PR_FLOAT3 ? 3 : 4);
        auto arr = &value;

        for (std::size_t index{}; index < arr_size; index++) {
          arr[index] = ReadValue<float>(bs);
        }

        break;
      }
      case PR_VECTOR:
      case PR_NORM_QUAT: {
        auto arr = reinterpret_cast<float *>(&value);

        if (type == PR_VECTOR) {
          bs->ReadVector(arr[0], arr[1], arr[2]);
        } else {
          bs->ReadNormQuat(arr[0], arr[1], arr[2], arr[3]);
        }

        break;
      }
      case PR_STRING8:
      case PR_STRING32: {
        const auto max_size = *GetPhysAddr(params[i + 3]) - 1;

        cell size{};

        if (type == PR_STRING8) {
          size = ReadValue<unsigned char>(bs);
        } else {
          size = ReadValue<unsigned int>(bs);
        }

        if (size > 0) {
          if (size > max_size) {
            Plugin::Log("%s: Warning! size (%d) > max_size (%d) "
                        "(PR_STRING8/PR_STRING32)",
                        __FUNCTION__, size, max_size);

            size = max_size;
          }

          std::unique_ptr<char[]> str{new char[size + 1]{}};

          bs->Read(str.get(), size);

          SetString(&value, str.get(), size + 1);
        }

        i++;

        break;
      }
      case PR_IGNORE_BITS: {
        bs->IgnoreBits(value);
        break;
      }
      default: {
        throw std::runtime_error{"Invalid type of value"};
      }
    }
  }

  return 1;
}

bool Script::OnLoad() {
  config_ = Plugin::Get().GetConfig();

  int num_publics{};
  pawn_script_->NumPublics(&num_publics);

  for (int index{}; index < num_publics; index++) {
    std::string public_name = GetPublicName(index);
    if (std::regex_match(public_name, regex_reg_handler_public_name_)) {
      publics_reg_handler_.push_back(MakePublic(public_name));
    } else if (public_name == "OnIncomingPacket") {
      InitPublic(PR_INCOMING_PACKET, public_name);
    } else if (public_name == "OnIncomingRPC") {
      InitPublic(PR_INCOMING_RPC, public_name);
    } else if (public_name == "OnOutgoingPacket") {
      InitPublic(PR_OUTGOING_PACKET, public_name);
    } else if (public_name == "OnOutgoingRPC") {
      InitPublic(PR_OUTGOING_RPC, public_name);
    }

    if (public_name == "OnOutcomingPacket") {
      public_on_outcoming_packet_ =
          MakePublic(public_name, config_->UseCaching());
    } else if (public_name == "OnOutcomingRPC") {
      public_on_outcoming_rpc_ = MakePublic(public_name, config_->UseCaching());
    }
  }

  return true;
}

bool Script::ExecPublic(const PublicPtr &pub, int player_id,
                        unsigned char event_id, BitStream *bs) {
  if (!pub || !pub->Exists()) {
    return true;
  }

  bs->resetReadPointer();

  cell bs_handle = bitstream_pool_.GetHandle(bs);
  bool is_external = (bs_handle == 0);
  if (is_external) {
    bs_handle = bitstream_pool_.AddExternal(bs);
  }

  cell result = pub->Exec(player_id, static_cast<cell>(event_id), bs_handle);

  if (is_external) {
    bitstream_pool_.RemoveExternal(bs_handle);
  }

  return result;
}

cell Script::CallbackExec(const PublicPtr &pub, int player_id, BitStream *bs) {
  if (!pub || !pub->Exists()) {
    return true;
  }

  bs->resetReadPointer();

  cell bs_handle = bitstream_pool_.GetHandle(bs);
  bool is_external = (bs_handle == 0);
  if (is_external) {
    bs_handle = bitstream_pool_.AddExternal(bs);
  }

  cell result = pub->Exec(player_id, bs_handle);

  if (is_external) {
    bitstream_pool_.RemoveExternal(bs_handle);
  }

  return result;
}

void Script::InitPublic(PR_EventType type, const std::string &public_name) {
  publics_.at(type) = MakePublic(public_name, config_->UseCaching());
}

void Script::InitHandler(unsigned char event_id,
                         const std::string &public_name, PR_EventType type) {
  auto &plugin = Plugin::Get();

  auto pub = MakePublic(public_name, config_->UseCaching());
  if (!pub->Exists()) {
    throw std::runtime_error{"Public " + public_name + " does not exist"};
  }

  handlers_.at(type).at(event_id).push_back(pub);

  if (type == PR_INCOMING_CUSTOM_RPC) {
    plugin.SetCustomRPC(event_id);
  }
}

void Script::InitHandlers() {
  for (const auto &pub : publics_reg_handler_) {
    if (pub && pub->Exists()) {
      pub->Exec();
    }
  }
}

BitStream *Script::GetBitStream(cell handle) {
  auto bs = bitstream_pool_.Get(handle);
  if (bs) {
    return bs;
  }

  bs = reinterpret_cast<BitStream *>(static_cast<uintptr_t>(handle));
  if (bs) {
    return bs;
  }

  throw std::runtime_error{"Invalid BitStream handle"};
}

template <typename T, bool compressed>
void Script::WriteValue(BitStream *bs, cell value) {
  T prepared_value{};

  if constexpr (std::is_same<float, T>::value) {
    prepared_value = amx_ctof(value);
  } else {
    prepared_value = static_cast<T>(value);
  }

  if constexpr (compressed) {
    bs->WriteCompressed<T>(prepared_value);
  } else {
    bs->Write<T>(prepared_value);
  }
}

template <typename T, bool compressed>
cell Script::ReadValue(BitStream *bs) {
  T value{};

  if constexpr (compressed) {
    bs->ReadCompressed<T>(value);
  } else {
    bs->Read<T>(value);
  }

  if constexpr (std::is_same<float, T>::value) {
    return amx_ftoc(value);
  }

  return static_cast<cell>(value);
}

template void Script::WriteValue<char, false>(BitStream *, cell);
template void Script::WriteValue<short, false>(BitStream *, cell);
template void Script::WriteValue<int, false>(BitStream *, cell);
template void Script::WriteValue<unsigned char, false>(BitStream *, cell);
template void Script::WriteValue<unsigned short, false>(BitStream *, cell);
template void Script::WriteValue<unsigned int, false>(BitStream *, cell);
template void Script::WriteValue<float, false>(BitStream *, cell);
template void Script::WriteValue<bool, false>(BitStream *, cell);
template void Script::WriteValue<char, true>(BitStream *, cell);
template void Script::WriteValue<short, true>(BitStream *, cell);
template void Script::WriteValue<int, true>(BitStream *, cell);
template void Script::WriteValue<unsigned char, true>(BitStream *, cell);
template void Script::WriteValue<unsigned short, true>(BitStream *, cell);
template void Script::WriteValue<unsigned int, true>(BitStream *, cell);
template void Script::WriteValue<float, true>(BitStream *, cell);
template void Script::WriteValue<bool, true>(BitStream *, cell);

template cell Script::ReadValue<char, false>(BitStream *);
template cell Script::ReadValue<short, false>(BitStream *);
template cell Script::ReadValue<int, false>(BitStream *);
template cell Script::ReadValue<unsigned char, false>(BitStream *);
template cell Script::ReadValue<unsigned short, false>(BitStream *);
template cell Script::ReadValue<unsigned int, false>(BitStream *);
template cell Script::ReadValue<float, false>(BitStream *);
template cell Script::ReadValue<bool, false>(BitStream *);
template cell Script::ReadValue<char, true>(BitStream *);
template cell Script::ReadValue<short, true>(BitStream *);
template cell Script::ReadValue<int, true>(BitStream *);
template cell Script::ReadValue<unsigned char, true>(BitStream *);
template cell Script::ReadValue<unsigned short, true>(BitStream *);
template cell Script::ReadValue<unsigned int, true>(BitStream *);
template cell Script::ReadValue<float, true>(BitStream *);
template cell Script::ReadValue<bool, true>(BitStream *);
