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
#include <Server/Components/Pawn/Impl/pawn_natives.hpp>

// native PR_Init();
SCRIPT_API(PR_Init, int())
{
  Plugin::GetScript(GetAMX()).PR_Init();
  return 1;
}

// native PR_RegHandler(eventid, const publicname[], PR_EventType:type);
SCRIPT_API(PR_RegHandler, int(int event_id, std::string const& public_name, int type))
{
  Plugin::GetScript(GetAMX()).PR_RegHandler(
      static_cast<unsigned char>(event_id), public_name,
      static_cast<PR_EventType>(type));
  return 1;
}

// native PR_SendPacket(BitStream:bs, playerid, ...);
SCRIPT_API(PR_SendPacket, int(BitStream& bs, int player_id, int priority, int reliability, int ordering_channel))
{
  auto core = PluginComponent::getCore();
  if (!core) return 0;

  if (player_id == -1)
  {
    core->getPlayers().broadcastPacket(
        Span<uint8_t>(bs.GetData(), bs.GetNumberOfBitsUsed()),
        static_cast<unsigned char>(ordering_channel), nullptr, false);
  }
  else
  {
    auto player = core->getPlayers().get(player_id);
    if (!player) return 0;

    if (!player->sendPacket(
            Span<uint8_t>(bs.GetData(), bs.GetNumberOfBitsUsed()),
            static_cast<unsigned char>(ordering_channel), false))
      return 0;
  }
  return 1;
}

// native PR_SendRPC(BitStream:bs, playerid, rpcid, ...);
SCRIPT_API(PR_SendRPC, int(BitStream& bs, int player_id, int rpc_id, int priority, int reliability, int ordering_channel))
{
  auto core = PluginComponent::getCore();
  if (!core) return 0;

  if (player_id == -1)
  {
    core->getPlayers().broadcastRPC(
        static_cast<RPCIndex>(rpc_id),
        Span<uint8_t>(bs.GetData(), bs.GetNumberOfBitsUsed()),
        static_cast<unsigned char>(ordering_channel), nullptr, false);
  }
  else
  {
    auto player = core->getPlayers().get(player_id);
    if (!player) return 0;

    if (!player->sendRPC(
            static_cast<RPCIndex>(rpc_id),
            Span<uint8_t>(bs.GetData(), bs.GetNumberOfBitsUsed()),
            static_cast<unsigned char>(ordering_channel), false))
      return 0;
  }
  return 1;
}

// native PR_EmulateIncomingPacket(BitStream:bs, playerid);
SCRIPT_API(PR_EmulateIncomingPacket, int(BitStream& bs, int player_id))
{
  auto core = PluginComponent::getCore();
  if (!core) return 0;

  auto player = core->getPlayers().get(player_id);
  if (!player) return 0;

  auto data = bs.GetData();
  if (!data) return 0;

  int packet_id = static_cast<int>(data[0]);

  for (auto network : core->getNetworks())
  {
    auto event_dispatcher =
        reinterpret_cast<Impl::DefaultEventDispatcher<NetworkInEventHandler>*>(
            &network->getInEventDispatcher());

    if (!event_dispatcher->stopAtFalse(
            [player, &bs, packet_id](NetworkInEventHandler* handler) {
              if (handler == PluginComponent::get()) return true;
              bs.SetReadOffset(8);
              return handler->onReceivePacket(*player, packet_id, bs);
            }))
      return 1;

    auto event_single_dispatcher = reinterpret_cast<
        Impl::DefaultIndexedEventDispatcher<SingleNetworkInEventHandler>*>(
        &network->getPerPacketInEventDispatcher());

    if (!event_single_dispatcher->stopAtFalse(
            packet_id, [player, &bs](SingleNetworkInEventHandler* handler) {
              bs.SetReadOffset(8);
              return handler->onReceive(*player, bs);
            }))
      return 1;
  }
  return 1;
}

// native PR_EmulateIncomingRPC(BitStream:bs, playerid, rpcid);
SCRIPT_API(PR_EmulateIncomingRPC, int(BitStream& bs, int player_id, int rpc_id))
{
  auto core = PluginComponent::getCore();
  if (!core) return 0;

  auto player = core->getPlayers().get(player_id);
  if (!player) return 0;

  for (auto network : core->getNetworks())
  {
    auto event_dispatcher =
        reinterpret_cast<Impl::DefaultEventDispatcher<NetworkInEventHandler>*>(
            &network->getInEventDispatcher());

    if (!event_dispatcher->stopAtFalse(
            [player, &bs, rpc_id](NetworkInEventHandler* handler) {
              if (handler == PluginComponent::get()) return true;
              bs.resetReadPointer();
              return handler->onReceiveRPC(*player, rpc_id, bs);
            }))
      return 1;

    auto event_single_dispatcher = reinterpret_cast<
        Impl::DefaultIndexedEventDispatcher<SingleNetworkInEventHandler>*>(
        &network->getPerRPCInEventDispatcher());

    if (!event_single_dispatcher->stopAtFalse(
            rpc_id, [player, &bs](SingleNetworkInEventHandler* handler) {
              bs.resetReadPointer();
              return handler->onReceive(*player, bs);
            }))
      return 1;
  }
  return 1;
}

// native BitStream:BS_New();
SCRIPT_API(BS_New, int())
{
  return Plugin::GetScript(GetAMX()).BS_New();
}

// native BitStream:BS_NewCopy(BitStream:bs);
SCRIPT_API(BS_NewCopy, int(BitStream& bs))
{
  return Plugin::GetScript(GetAMX()).BS_NewCopy(&bs);
}

// native BS_Delete(&BitStream:bs);
SCRIPT_API(BS_Delete, int(cell& bs))
{
  auto handle = static_cast<cell>(bs);
  Plugin::GetScript(GetAMX()).BS_Delete(&handle);
  bs = 0;
  return 1;
}

// native BS_Reset(BitStream:bs);
SCRIPT_API(BS_Reset, int(BitStream& bs))
{
  bs.reset();
  return 1;
}

// native BS_ResetReadPointer(BitStream:bs);
SCRIPT_API(BS_ResetReadPointer, int(BitStream& bs))
{
  bs.resetReadPointer();
  return 1;
}

// native BS_ResetWritePointer(BitStream:bs);
SCRIPT_API(BS_ResetWritePointer, int(BitStream& bs))
{
  bs.resetWritePointer();
  return 1;
}

// native BS_IgnoreBits(BitStream:bs, number_of_bits);
SCRIPT_API(BS_IgnoreBits, int(BitStream& bs, int number_of_bits))
{
  bs.IgnoreBits(number_of_bits);
  return 1;
}

// native BS_SetWriteOffset(BitStream:bs, offset);
SCRIPT_API(BS_SetWriteOffset, int(BitStream& bs, int offset))
{
  bs.SetWriteOffset(offset);
  return 1;
}

// native BS_GetWriteOffset(BitStream:bs, &offset);
SCRIPT_API(BS_GetWriteOffset, int(BitStream& bs, cell& offset))
{
  offset = bs.GetWriteOffset();
  return 1;
}

// native BS_SetReadOffset(BitStream:bs, offset);
SCRIPT_API(BS_SetReadOffset, int(BitStream& bs, int offset))
{
  bs.SetReadOffset(offset);
  return 1;
}

// native BS_GetReadOffset(BitStream:bs, &offset);
SCRIPT_API(BS_GetReadOffset, int(BitStream& bs, cell& offset))
{
  offset = bs.GetReadOffset();
  return 1;
}

// native BS_GetNumberOfBitsUsed(BitStream:bs, &number);
SCRIPT_API(BS_GetNumberOfBitsUsed, int(BitStream& bs, cell& number))
{
  number = bs.GetNumberOfBitsUsed();
  return 1;
}

// native BS_GetNumberOfBytesUsed(BitStream:bs, &number);
SCRIPT_API(BS_GetNumberOfBytesUsed, int(BitStream& bs, cell& number))
{
  number = bs.GetNumberOfBytesUsed();
  return 1;
}

// native BS_GetNumberOfUnreadBits(BitStream:bs, &number);
SCRIPT_API(BS_GetNumberOfUnreadBits, int(BitStream& bs, cell& number))
{
  number = bs.GetNumberOfUnreadBits();
  return 1;
}

// native BS_GetNumberOfBitsAllocated(BitStream:bs, &number);
SCRIPT_API(BS_GetNumberOfBitsAllocated, int(BitStream& bs, cell& number))
{
  number = bs.GetNumberOfBitsAllocated();
  return 1;
}

// BS_WriteValue and BS_ReadValue use manual registration (varargs via cell* params).
static cell AMX_NATIVE_CALL BS_WriteValueNative(AMX* amx, cell* params)
{
  try
  {
    auto& script = Plugin::GetScript(amx);
    return script.BS_WriteValue(params);
  }
  catch (const std::exception& e)
  {
    Plugin::Log("BS_WriteValue: %s", e.what());
  }
  return 0;
}

static cell AMX_NATIVE_CALL BS_ReadValueNative(AMX* amx, cell* params)
{
  try
  {
    auto& script = Plugin::GetScript(amx);
    return script.BS_ReadValue(params);
  }
  catch (const std::exception& e)
  {
    Plugin::Log("BS_ReadValue: %s", e.what());
  }
  return 0;
}

AMX_NATIVE_INFO manual_natives[] = {
    {"BS_WriteValue", BS_WriteValueNative},
    {"BS_ReadValue", BS_ReadValueNative},
    {nullptr, nullptr},
};
