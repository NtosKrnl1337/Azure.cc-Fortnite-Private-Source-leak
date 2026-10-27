#pragma once
// Generated from sdk.h — sdk.h

// Simple offsets
#define GENGINE             0x1B2C7518
#define VIEWPORT            0xB70
#define GAME_INSTANCE       0x238  // UWorld -> OwningGameInstance
#define GAME_STATE          0x1C0  // UWorld -> GameState
#define LOCAL_PLAYERS       0x38  // UGameInstance -> LocalPlayers
#define PLAYER_CONTROLLER   0x30  // UPlayer -> PlayerController
#define PLAYER_STATE        0x290  // AController -> PlayerState
#define LOCAL_PAWN          0x318  // APlayerController -> AcknowledgedPawn
#define PLAYER_ARRAY        0x288  // AGameStateBase -> PlayerArray
#define NETCONNECTION       0x4A8  // APlayerController -> NetConnection
#define PAWN_PRIVATE        0x2E8  // APlayerState -> PawnPrivate
#define TEAM_INDEX          0xF69  // AFortPlayerStateAthena -> TeamIndex
#define HABANERO_COMPONENT  0x918  // AFortPlayerState -> HabaneroComponent
#define RANKED_PROGRESS     0xD8  // UFortPlayerStateComponent_Habanero -> RankedProgress
#define PLATFORM            0x400  // AFortPlayerState -> Platform
#define PLAYERNAME          0x308  // AFortPlayerState -> PlayerNamePrivate
#define MESH                0x2F0  // ACharacter -> Mesh
#define CURRENT_WEAPON      0x9A0  // AFortPawn -> CurrentWeapon
#define WEAPON_DATA         0x638  // AFortWeapon -> WeaponData
#define WEAPON_NAME         0x38  // UItemDefinitionBase -> ItemName
#define AMMO_COUNT          0x1100  // AFortWeapon -> AmmoCount
#define BONE_ARRAY          0x660
#define ROOT_COMPONENT      0x1B0  // AActor -> RootComponent
#define RELATIVE_LOCATION   0x140  // USceneComponent -> RelativeLocation
#define COMPONENT_TO_WORLD  0x1E0
#define CAMPOS              0x168
#define CAMROT              CAMPOS + 0x10
#define FOV                 0x374
#define SECONDS             CAMROT + 0x10
