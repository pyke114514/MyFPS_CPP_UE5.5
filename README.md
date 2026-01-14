# MyFPS_CPP_UE5.5
使用UE5.5制作的FPS游戏练习Demo，为C++项目。

WASD移动

Shift奔跑

鼠标左键开火

鼠标右键瞄准

Q切枪

R换弹

V检视

G丢枪


其他说明：

游戏支持局域网联机，房主根据设备ip地址创建房间，其余玩家通过输入ip地址加入房间。

每个房间8位玩家，如果人数不足会由AI玩家补齐。

游戏分为3个阶段：
	30s热身，自动生成一个AI供玩家对战，同时等待其他玩家进入。
	10min比赛，玩法为简单的个人死斗模式，击杀其他玩家获得积分，爆头击杀积分翻倍。
	10s结算，根据积分排名，并显示Top3的玩家名。

爆头双倍伤害。

移动速度较大时开火或持续开火会导致较大的弹道扩散。



A FPS game practice demo created using UE5.5, for a C++ project.

WASD Movement.

Shift is running. 

Left-click to fire. 

Right-click aiming 

Q Switching guns 

R reloads the ammunition 

V Inspection 

G dropped the gun 


Other notes: 

The game supports local network multiplayer. The host creates the room based on the device's IP address, and other players join the room by entering the IP address. 

Each room accommodates 8 players. If the number of players is insufficient, AI players will be added to complete the team. 

The game is divided into 3 stages:
  30 seconds of warm-up, an AI is automatically generated for the players to compete against, and at the same time, other players are waiting to join.
  10 minutes of competition, the gameplay is a simple individual deathmatch mode. Killing other players earns points, and headshot kills double the points.
  10 seconds of settlement, based on the ranking of points, and the names of the top 3 players are displayed. 

Headshot deals double damage. 

When moving at a high speed, firing or continuously firing will result in a larger trajectory dispersion.
