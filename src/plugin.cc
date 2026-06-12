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
#include <Server/Components/Pawn/Impl/pawn_impl.hpp>

// Declared in natives.cpp
extern AMX_NATIVE_INFO manual_natives[];

Plugin &Plugin::Instance() {
  static Plugin instance;
  return instance;
}

bool Plugin::Load(IPawnComponent *pawn_component, ICore *core) {
  auto &plugin = Instance();

  plugin.core_ = core;
  plugin.pawn_component_ = pawn_component;

  try {
    core->logLn(LogLevel::Message, "[%s] plugin v%s loading...",
                plugin.Name(), plugin.VersionAsString().c_str());

    setAmxFunctions(pawn_component->getAmxFunctions());
    setAmxLookups(core);

    plugin.OnLoad();

    return true;
  } catch (const std::exception &e) {
    core->logLn(LogLevel::Error, "[%s] %s: %s", plugin.Name(), __func__,
                e.what());
  }

  return false;
}

void Plugin::Unload() {
  auto &plugin = Instance();

  try {
    plugin.OnUnload();
  } catch (const std::exception &e) {
    plugin.core_->logLn(LogLevel::Error, "[%s] %s: %s", plugin.Name(),
                        __func__, e.what());
  }
}

void Plugin::AddScript(IPawnScript *pawn_script) {
  auto &plugin = Instance();
  AMX *amx = pawn_script->GetAMX();

  try {
    // Create or get ScriptData entry (default constructs if not exist)
    auto &data = plugin.scripts_data_[amx];
    data.SetAMX(amx);

    // Add to script list for EveryScript iteration
    plugin.script_list_.push_back(pawn_script);

    pawn_natives::AmxLoad(amx);

    int num_manual = 0;
    while (manual_natives[num_manual].name != nullptr)
      num_manual++;
    if (num_manual > 0)
      pawn_script->Register(manual_natives, num_manual);

    data.OnLoad();
  } catch (const std::exception &e) {
    plugin.core_->logLn(LogLevel::Error, "[%s] %s: %s", plugin.Name(),
                        __func__, e.what());
  }
}

void Plugin::RemoveScript(IPawnScript *pawn_script) {
  auto &plugin = Instance();
  AMX *amx = pawn_script->GetAMX();

  // Free BitStreams owned by this script
  plugin.pool_.FreeByOwner(amx);

  // Remove ScriptData
  plugin.scripts_data_.erase(amx);

  // Remove from script list
  auto it = std::find(plugin.script_list_.begin(),
                      plugin.script_list_.end(), pawn_script);
  if (it != plugin.script_list_.end()) {
    plugin.script_list_.erase(it);
  }
}

void Plugin::ProcessTick() {
  auto &plugin = Instance();

  try {
    plugin.OnProcessTick();
  } catch (const std::exception &e) {
    plugin.core_->logLn(LogLevel::Error, "[%s] %s: %s", plugin.Name(),
                        __func__, e.what());
  }
}

ScriptData &Plugin::GetScriptData(AMX *amx) {
  auto &data = Instance().scripts_data_[amx];
  data.SetAMX(amx);
  return data;
}

std::tuple<int, int, int> Plugin::VersionToTuple(int version) {
  return std::make_tuple((version >> 16) & 0xFF, (version >> 8) & 0xFF,
                         version & 0xFF);
}

std::string Plugin::VersionAsString() const {
  auto [major, minor, patch] = VersionToTuple(Version());

  return std::to_string(major) + "." + std::to_string(minor) + "." +
         std::to_string(patch);
}

bool Plugin::OnLoad() {
  config_ = std::make_shared<Config>("components/pawnraknet.cfg");

  config_->Read();

  Log("\n\n"
      "    | %s %s | open.mp | 2016 - %s"
      "\n"
      "    |--------------------------------------------"
      "\n"
      "    | Author and maintainer: katursis"
      "\n\n\n"
      "    | Compiled: %s at %s"
      "\n"
      "    |--------------------------------------------------------------"
      "\n"
      "    | Repository: https://github.com/katursis/%s/tree/omp"
      "\n"
      "    |--------------------------------------------------------------"
      "\n"
      "    | Wiki: https://github.com/katursis/%s/wiki"
      "\n",
      Name(), VersionAsString().c_str(), &__DATE__[7], __DATE__, __TIME__,
      Name(), Name());

  return true;
}

void Plugin::OnUnload() {
  config_->Save();

  Log("plugin unloaded");
}

void Plugin::OnProcessTick() {}

void Plugin::SetCustomRPC(RPCIndex rpc_id) { custom_rpc_[rpc_id] = true; }

bool Plugin::IsCustomRPC(RPCIndex rpc_id) { return custom_rpc_[rpc_id]; }

const std::shared_ptr<Config> &Plugin::GetConfig() { return config_; }
