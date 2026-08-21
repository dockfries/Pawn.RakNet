# Pawn.RakNet
[![GitHub Release](https://img.shields.io/github/release/dockfries/Pawn.RakNet.svg)](https://github.com/dockfries/Pawn.RakNet/releases/latest)

[English](README.md) | **简体中文**

**Open Multiplayer** 服务器组件，允许你捕获和分析 RakNet 网络流量

> ⚠️ 深度修改的 Fork
>
> 本仓库包含大量修改，旨在无需 polyfill 的情况下与 infernus 配合使用。

## 主要特性
* 捕获、修改、过滤传入/传出的数据包（packets）和 RPC
* 向玩家发送自定义的数据包和 RPC
* 模拟来自玩家的传入数据包和 RPC

## 文档

[Pawn.RakNet 维基](https://github.com/katursis/Pawn.RakNet/wiki)

[官方 RakNet 手册](http://www.jenkinssoftware.com/raknet/manual/index.html)

## 简单示例
```pawn
const PLAYER_SYNC = 207;

IPacket:PLAYER_SYNC(playerid, BitStream:bs)
{
  new onFootData[PR_OnFootSync];

  BS_IgnoreBits(bs, 8); // 忽略数据包 id (uint8)
  BS_ReadOnFootSync(bs, onFootData);

  printf(
    "PLAYER_SYNC[%d]:\nlrKey %d \nudKey %d \nkeys %d \nposition %.2f %.2f %.2f \nquaternion %.2f %.2f %.2f %.2f \nhealth %d \narmour %d \nadditionalKey %d \nweaponId %d \nspecialAction %d \nvelocity %.2f %.2f %.2f \nsurfingOffsets %.2f %.2f %.2f \nsurfingVehicleId %d \nanimationId %d \nanimationFlags %d",
    playerid,
    onFootData[PR_lrKey],
    onFootData[PR_udKey],
    onFootData[PR_keys],
    onFootData[PR_position][0],
    onFootData[PR_position][1],
    onFootData[PR_position][2],
    onFootData[PR_quaternion][0],
    onFootData[PR_quaternion][1],
    onFootData[PR_quaternion][2],
    onFootData[PR_quaternion][3],
    onFootData[PR_health],
    onFootData[PR_armour],
    onFootData[PR_additionalKey],
    onFootData[PR_weaponId],
    onFootData[PR_specialAction],
    onFootData[PR_velocity][0],
    onFootData[PR_velocity][1],
    onFootData[PR_velocity][2],
    onFootData[PR_surfingOffsets][0],
    onFootData[PR_surfingOffsets][1],
    onFootData[PR_surfingOffsets][2],
    onFootData[PR_surfingVehicleId],
    onFootData[PR_animationId],
    onFootData[PR_animationFlags]
  );

  return 1;
}
```
