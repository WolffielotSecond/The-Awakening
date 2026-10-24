#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputCoreTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Core/TAGamepadBrandResolver.h"
#include "TAInputIconSubsystem.generated.h"

class UInputAction;
class UTexture2D;
class ULocalPlayer;
class UEnhancedInputLocalPlayerSubsystem;

UENUM(BlueprintType)
enum class EInputDeviceType : uint8
{
	KeyboardMouse,
	Xbox,
	PS5,
	Switch
};

/** 设置“控制器样式”：自动按检测到的手柄切换，或强制使用一套手柄图标。 */
UENUM(BlueprintType)
enum class ETAGamepadIconStyle : uint8
{
	Auto,
	Xbox,
	DualSense
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInputDeviceChanged);

UCLASS(Config = Game)
class THE_AWAKENING_API UTAInputIconSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 获取指定按键对应的图标 */
	UFUNCTION(BlueprintCallable, Category = "InputIcon")
	UTexture2D* GetIconForKey(FKey Key) const;

	/** 获取指定按键对应的手柄图标，未检测到手柄类型时使用 Xbox 图标。 */
	UTexture2D* GetGamepadIconForKey(FKey Key) const;

	/** 获取指定 InputAction 当前绑定按键的图标 */
	UFUNCTION(BlueprintCallable, Category = "InputIcon")
	UTexture2D* GetIconForAction(UInputAction* Action) const;
	/** Query this widget's player's active IMC bindings, filtered to the current device. */
	UTexture2D* GetIconForActionForPlayer(UInputAction* Action, ULocalPlayer* LocalPlayer) const;
	TArray<FKey> GetPromptKeysForAction(UInputAction* Action, ULocalPlayer* LocalPlayer) const;
	UTexture2D* GetStickDirectionIcon(int32 Horizontal, int32 Vertical) const;

	/** 当前输入设备类型 */
	UFUNCTION(BlueprintCallable, Category = "InputIcon")
	EInputDeviceType GetCurrentDeviceType() const { return CurrentDeviceType; }

	/** 手动设置当前输入设备类型 */
	UFUNCTION(BlueprintCallable, Category = "InputIcon")
	void SetCurrentDeviceType(EInputDeviceType NewType);

	/** 根据实际按下的按键自动更新设备类型 */
	UFUNCTION(BlueprintCallable, Category = "InputIcon")
	void NotifyInputKey(const FKey& Key);

	/** 从引擎记录的最近输入硬件同步当前设备类型 */
	void RefreshCurrentDeviceForUser(FPlatformUserId UserId);

	/** 控制器样式设置；只影响手柄图标，键鼠模式不受影响。 */
	UFUNCTION(BlueprintCallable, Category = "InputIcon")
	void SetGamepadIconStyle(ETAGamepadIconStyle NewStyle);
	UFUNCTION(BlueprintPure, Category = "InputIcon")
	ETAGamepadIconStyle GetGamepadIconStyle() const { return GamepadIconStyle; }

	/** 清空手柄品牌缓存，下一次手柄输入重新识别。手柄连接变化时自动调用。 */
	void InvalidateGamepadBrands();

	/** 替换品牌识别（自动化测试用）；传空函数恢复默认识别器。 */
	void SetGamepadBrandProbe(TFunction<ETAGamepadBrand(int32)> Probe);
	void SetTimeSourceForTests(TFunction<double()> Source) { TimeSource = MoveTemp(Source); }


	UPROPERTY(BlueprintAssignable, Category = "InputIcon")
	FOnInputDeviceChanged OnInputDeviceChanged;

	/** Shared prompt refresh: device, language, or rebuilt input mappings. */
	UPROPERTY(BlueprintAssignable, Category = "InputIcon")
	FOnInputDeviceChanged OnInputPromptsChanged;
	UFUNCTION(BlueprintCallable, Category = "InputIcon")
	void NotifyInputPromptsChanged();

protected:
	FString BuildIconPath(FKey Key, EInputDeviceType Device) const;
	UTexture2D* LoadIconFromPath(const FString& Path) const;
	UTexture2D* LoadIconForDevice(FKey Key, EInputDeviceType Device) const;
	EInputDeviceType GetGamepadDeviceType() const;
	/** 解析当前手柄槽位的品牌：命中缓存直接返回；否则识别，识别不出时按间隔补查。 */
	void UpdateGamepadBrand(int32 XInputSlot, bool bForceProbe);
	void ApplyDeviceType(EInputDeviceType NewType);
	void HandleInputDeviceConnectionChange(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);
	static int32 GetCurrentXInputSlot();
	FString KeyToAssetName(FKey Key) const;
	FString XboxAssetName(const FString& Logical) const;
	FString PSAssetName(const FString& Logical) const;
	void RegisterLocalPlayer(ULocalPlayer* LocalPlayer);
	void UnregisterLocalPlayer(ULocalPlayer* LocalPlayer);
	TArray<TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem>> ObservedInputSubsystems;

protected:
	UPROPERTY()
	EInputDeviceType CurrentDeviceType = EInputDeviceType::KeyboardMouse;

	UPROPERTY(EditDefaultsOnly, Category = "InputIcon")
	FString IconRootPath = TEXT("/Game/Textures/Keys");

	/** 品牌识别不出（如 Steam 句柄尚未就绪）时，再次识别的最小间隔（秒）。 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "InputIcon", meta = (ClampMin = "0"))
	float GamepadBrandRetryInterval = 0.5f;

	/** PS 图标缺失时是否改用同一按键的 Xbox 图标。 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "InputIcon")
	bool bFallbackToXboxIcons = true;

	ETAGamepadIconStyle GamepadIconStyle = ETAGamepadIconStyle::Auto;
	ETAGamepadBrand DetectedGamepadBrand = ETAGamepadBrand::Unknown;
	TMap<int32, ETAGamepadBrand> GamepadBrandCache;
	int32 LastGamepadSlot = INDEX_NONE;
	double LastBrandProbeTime = -1.0;
	FTAGamepadBrandResolver BrandResolver;
	TFunction<ETAGamepadBrand(int32)> BrandProbe;
	TFunction<double()> TimeSource;
	FDelegateHandle DeviceConnectionHandle;

	/** 已加载的图标（强引用防止被回收）与确认缺失的路径，避免重复同步加载。 */
	mutable TMap<FString, TStrongObjectPtr<UTexture2D>> IconCache;
	mutable TSet<FString> MissingIconPaths;
};
