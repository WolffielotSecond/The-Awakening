#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Health/TADamageTypes.h"
#include "Styling/SlateBrush.h"
#include "TACheatManager.generated.h"

class UAbilitySystemComponent;
class UTAHealthComponent;

/**
 * 项目调试命令。CheatManager 只在允许作弊时创建（PIE / 开发版），Shipping 包自动没有。
 * 输出只写日志，属于开发者文本，不接本地化。所有伤害来源为 Damage.Source.Console。
 */
UCLASS()
class THE_AWAKENING_API UTACheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** TADamage <Amount>：普通伤害，验证修改器、防御和保护。 */
	UFUNCTION(Exec)
	void TADamage(float Amount);

	/** TATrueDamage <Amount>：真实伤害。 */
	UFUNCTION(Exec)
	void TATrueDamage(float Amount);

	/** TAKill：强制伤害，扣掉全部 Health，任何保护都挡不住。 */
	UFUNCTION(Exec)
	void TAKill();

	/** TAHeal [Amount]：回血，不填或 <= 0 时回满。回血不是伤害，直接改 Health；死亡状态下无效（不能用来复活）。 */
	UFUNCTION(Exec)
	void TAHeal(float Amount = 0.f);

	/** TAGod：切换一条永久无敌保护（不挡强制伤害）。 */
	UFUNCTION(Exec)
	void TAGod();

	/** TACheckpoint <Id>：直接设为当前复活点，不用走过去交互。 */
	UFUNCTION(Exec)
	void TACheckpoint(FName CheckpointId);

	/** TAAction <Tag>：播放角色动作（例如 Anim.Pickup），日志输出关键帧和结束；不写 Tag 时打断当前动作。 */
	UFUNCTION(Exec)
	void TAAction(const FString& ActionTag = TEXT(""));

	/** TAPreview [MeshPath | off]：在屏幕左上角显示 3D 预览台画面（静态或骨骼网格体）；不填路径显示占位立方体，off 关闭。 */
	UFUNCTION(Exec)
	void TAPreview(const FString& MeshPath = TEXT(""));

	/** TAGiveDiary：把项目设置里配置的日记本放进剧情槽。 */
	UFUNCTION(Exec)
	void TAGiveDiary();
	/** Spawn the reusable QTE tester near the current player. */
	UFUNCTION(Exec) void TASpawnQTE();

	/** TADocList：在日志和屏幕上列出已收集（按页序）和已读的文本。 */
	UFUNCTION(Exec)
	void TADocList();

	/** TADocReset：清空文本收集、已读记录和日记本页码（不会恢复已销毁的场景文本，重新开始 PIE 即可）。 */
	UFUNCTION(Exec)
	void TADocReset();

	/** TASave：立即存档到默认槽 */
	UFUNCTION(Exec)
	void TASave();

	/** TALoad：从默认槽读档（主菜单完成前的读档入口） */
	UFUNCTION(Exec)
	void TALoad();

	/** TASaveDelete：删除默认槽的存档 */
	UFUNCTION(Exec)
	void TASaveDelete();

	/** TASaveInfo：在日志和屏幕上显示存档头（版本、时间、关卡、复活点、文本数） */
	UFUNCTION(Exec)
	void TASaveInfo();

	bool IsGodModeEnabled() const { return GodProtection.IsValid(); }

	/** 关卡切换或 PIE 结束时控制器被销毁：先把调试画面从视口移除，避免 Slate 持有失效的画刷 */
	virtual void BeginDestroy() override;

private:
	UAbilitySystemComponent* GetPlayerASC() const;
	UTAHealthComponent* GetPlayerHealth() const;
	void ApplyConsoleDamage(float Amount, ETADamageType Type, const TCHAR* Command);

	// 只持有自己这一条保护，关闭时只移除它，不影响对话、小游戏、复活保护等其他来源
	FTAHealthProtectionHandle GodProtection;
	TWeakObjectPtr<UTAHealthComponent> GodHealth;

	// TAPreview：直接用 Slate 贴到视口（UUserWidget 是抽象类，不能不经 WBP 直接创建）
	void CloseDebugPreview(bool bDestroyStage);
	TWeakObjectPtr<class ATAPreviewStage> DebugPreviewStage;
	TSharedPtr<class SWidget> DebugPreviewSlate;
	TWeakObjectPtr<class UGameViewportClient> DebugPreviewViewport;
	/** UPROPERTY：让画刷引用的 RenderTarget 在画面移除前不被回收 */
	UPROPERTY(Transient)
	FSlateBrush DebugPreviewBrush;
};
