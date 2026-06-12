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

#ifndef PAWNRAKNET_NATIVE_PARAMS_HPP_
#define PAWNRAKNET_NATIVE_PARAMS_HPP_

// amx_SetStringLen is defined in pawn_impl.hpp (included only from plugin.cc).
// Provide declaration before pawn_natives.hpp for template instantiation in NativeCast.hpp.
int amx_SetStringLen(cell* dest, const char* source, int length, int pack, int use_wchar, size_t size);

#include <Server/Components/Pawn/Impl/pawn_natives.hpp>

template <>
class pawn_natives::ParamCast<BitStream&>
{
public:
  ParamCast(AMX* amx, cell* params, int idx) noexcept
  {
    try
    {
      value_ = Plugin::Instance().GetPool().Get(params[idx]);
      if (value_ == nullptr) {
        // Fallback: direct pointer reinterpretation for callback BitStreams
        value_ = reinterpret_cast<BitStream*>(static_cast<uintptr_t>(params[idx]));
        if (value_ == nullptr) error_ = true;
      }
    }
    catch (const std::exception &e)
    {
      Plugin::Log("ParamCast<BitStream&>: %s", e.what());
      error_ = true;
    }
  }

  ~ParamCast() = default;
  ParamCast(ParamCast const&) = delete;
  ParamCast(ParamCast&&) = delete;

  operator BitStream&() { return *value_; }

  bool Error() const { return error_; }

  static constexpr int Size = 1;

private:
  BitStream* value_{};
  bool error_ = false;
};

template <>
class pawn_natives::ParamCast<BitStream*>
{
public:
  ParamCast(AMX* amx, cell* params, int idx) noexcept
  {
    try
    {
      value_ = Plugin::Instance().GetPool().Get(params[idx]);
      if (value_ == nullptr) {
        value_ = reinterpret_cast<BitStream*>(static_cast<uintptr_t>(params[idx]));
      }
    }
    catch (const std::exception &e)
    {
      Plugin::Log("ParamCast<BitStream*>: %s", e.what());
    }
  }

  ~ParamCast() = default;
  ParamCast(ParamCast const&) = delete;
  ParamCast(ParamCast&&) = delete;

  operator BitStream*() { return value_; }

  bool Error() const { return false; }

  static constexpr int Size = 1;

private:
  BitStream* value_{};
};

#endif
