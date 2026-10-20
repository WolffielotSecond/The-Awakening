#include "Document/TAWorldDocument.h"
#include "Inventory/TAItemSettings.h"
#include "The_AwakeningCharacter.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentSettings.h"
#include "Document/TADocumentSubsystem.h"
#include "Document/TACharacterActionComponent.h"
#include "Core/TAGameplayTags.h"
#include "Core/TALocalizeSubsystem.h"
#include "UI/Document/TADocumentReaderWidget.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "Scan/TAScannableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Inventory/TAItemSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTAWorldDocument, Log, All);

namespace
{
	const FName KeyHideWorldMesh(TEXT("HideWorldMesh"));
	const FName KeyShowWorldMesh(TEXT("ShowWorldMesh"));
}

ATAWorldDocument::ATAWorldDocument()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PlaceholderMesh = CubeAsset.Object;

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(RootComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetVisibility(false);
}

void ATAWorldDocument::ApplyFallbackMesh()
{
	if (!bUsesSkeletalMesh)
	{
		Super::ApplyFallbackMesh();
	}
}

void ATAWorldDocument::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!MeshComponent)
	{
		return;
	}
	UObject* Model = Definition ? Definition->Mesh.Get() : nullptr;
	USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Model);
	UStaticMesh* Static = Cast<UStaticMesh>(Model);
	bUsesSkeletalMesh = Skeletal != nullptr;
	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->SetSkeletalMeshAsset(Skeletal);
		SkeletalMeshComponent->SetVisibility(bUsesSkeletalMesh);
		if (Skeletal)
		{
			SkeletalMeshComponent->SetRelativeTransform(Definition->WorldTransform);
		}
	}
	if (bUsesSkeletalMesh)
	{
		MeshComponent->SetStaticMesh(nullptr);
		MeshComponent->SetVisibility(false);
	}
	else if (Static)
	{
		MeshComponent->SetStaticMesh(Static);
		MeshComponent->SetRelativeTransform(Definition->WorldTransform);
		MeshComponent->SetVisibility(true);
	}
	else
	{
		// 占位：一张平放的纸
		MeshComponent->SetStaticMesh(PlaceholderMesh);
		MeshComponent->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.3f, 0.21f, 0.01f)));
		MeshComponent->SetVisibility(true);
	}
}

void ATAWorldDocument::BeginPlay()
{
	// 先于基类判断：基类 BeginPlay 会给空的静态网格补占位模型（PIE 复制的 Actor 不保留这个运行时标记）
	bUsesSkeletalMesh = Definition && Cast<USkeletalMesh>(Definition->Mesh.Get());
	Super::BeginPlay();
	if (!Definition)
	{
		UE_LOG(LogTAWorldDocument, Warning, TEXT("%s has no document definition."), *GetName());
		return;
	}
	// 已收集的文本不再出现在场景里（关卡重新加载、复活后）
	if (const UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this); Documents && Documents->IsCollected(Definition->DocumentId))
	{
		Destroy();
		return;
	}
	if (bUsesSkeletalMesh)
	{
		UTAItemSettings::ApplyMissingMaterialFallback(SkeletalMeshComponent);
	}
	if (ScannableComponent)
	{
		FTAScanTargetInfo Info = ScannableComponent->GetScanInfo();
		Info.Name = FText::FromString(Definition->TitleTextId);
		// 目前所有文本统一用同一句介绍（项目设置 The Awakening Documents 可关闭）
		const UTADocumentSettings* Settings = GetDefault<UTADocumentSettings>();
		Info.Description = FText::FromString(Settings->bUseUnifiedScanDescription ? Settings->UnifiedScanDescriptionTextId : Definition->ScanDescriptionTextId);
		Info.TargetType = ETAScanTargetType::Item;
		ScannableComponent->SetScanInfo(Info);
	}
}

void ATAWorldDocument::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbortReading();
	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this))
	{
		Rollback->CommitScope(ReadingScope);
	}
	Super::EndPlay(EndPlayReason);
}

FString ATAWorldDocument::GetDisplayNameTextId() const
{
	return Definition ? Definition->TitleTextId : FString();
}

UTexture2D* ATAWorldDocument::GetDisplayIcon() const
{
	return Definition ? UTAItemSettings::ResolveIcon(Definition->Icon.Get()) : nullptr;
}

bool ATAWorldDocument::CanInteract_Implementation(AActor* Interactor) const
{
	return Definition && !Definition->DocumentId.IsNone() && Interactor && State == EState::Idle;
}

void ATAWorldDocument::OnInteract_Implementation(AActor* Interactor)
{
	if (!ITAInteractable::Execute_CanInteract(this, Interactor))
	{
		return;
	}
	if (HasDiary(Interactor))
	{
		CollectPath(Interactor);
	}
	else
	{
		BeginReadPath(Interactor);
	}
}

bool ATAWorldDocument::HasDiary(AActor* Interactor) const
{
	const UTAItemDefinition* Diary = GetDefault<UTADocumentSettings>()->DiaryItem.LoadSynchronous();
	if (!Diary)
	{
		UE_LOG(LogTAWorldDocument, Warning, TEXT("Diary item is not set in Project Settings > The Awakening Documents; documents can only be read."));
		return false;
	}
	const UTAInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UTAInventoryComponent>() : nullptr;
	return Inventory && Inventory->HasStoryItem(Diary);
}

// ==================== 有日记本：收集 ====================

void ATAWorldDocument::CollectPath(AActor* Interactor)
{
	// 数据在交互帧完成；动画只是表现
	if (UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this))
	{
		Documents->Collect(Definition->DocumentId);
	}
	if (UTACharacterActionComponent* Action = Interactor->FindComponentByClass<UTACharacterActionComponent>())
	{
		Action->PlayAction(TAGameplayTags::Anim_InsertPage);
	}
	// 屏幕右侧拾取提示
	if (AThe_AwakeningCharacter* Character = Cast<AThe_AwakeningCharacter>(Interactor))
	{
		Character->NotifyDocumentCollected(Definition);
	}
	UE_LOG(LogTAWorldDocument, Log, TEXT("Collected document '%s'."), *Definition->DocumentId.ToString());
	Destroy();
}

// ==================== 没有日记本：拿起阅读再放回 ====================

void ATAWorldDocument::BeginReadPath(AActor* Interactor)
{
	State = EState::PickingUp;
	const uint32 Serial = ++FlowSerial;
	ReaderInstigator = Interactor;

	// 阅读期间死亡：已读旗标撤销，界面关闭，模型恢复
	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this))
	{
		ReadingScope = Rollback->BeginScope(this, TEXT("ReadDocument"),
			[WeakThis = TWeakObjectPtr<ATAWorldDocument>(this)] { if (WeakThis.IsValid()) WeakThis->AbortReading(); });
	}
	if (UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this))
	{
		Documents->MarkRead(Definition->DocumentId);
	}

	UTACharacterActionComponent* Action = Interactor->FindComponentByClass<UTACharacterActionComponent>();
	if (!Action)
	{
		SetWorldMeshVisible(false);
		OpenReader();
		return;
	}
	BindAction(Action);
	Action->PlayAction(TAGameplayTags::Anim_Pickup, [WeakThis = TWeakObjectPtr<ATAWorldDocument>(this), Serial](bool bInterrupted)
	{
		ATAWorldDocument* Self = WeakThis.Get();
		if (!Self || Self->FlowSerial != Serial || Self->State != EState::PickingUp)
		{
			return;
		}
		if (bInterrupted)
		{
			// 非死亡的打断：放弃这次阅读，保留已读记录
			Self->AbortReading();
			if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(Self))
			{
				Rollback->CommitScope(Self->ReadingScope);
			}
			return;
		}
		Self->OpenReader();
	});
}

void ATAWorldDocument::OpenReader()
{
	State = EState::Reading;
	SetWorldMeshVisible(false);

	// 阅读界面自己申请时停和输入；关闭（走完关闭动画）后回到这里播放放下动画
	const APawn* Pawn = Cast<APawn>(ReaderInstigator.Get());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const uint32 Serial = FlowSerial;
	Reader = UTADocumentReaderWidget::Open(PC, nullptr, Definition, UTADocumentReaderWidget::StandaloneInputPriority, true,
		[WeakThis = TWeakObjectPtr<ATAWorldDocument>(this), Serial](bool bForced)
		{
			ATAWorldDocument* Self = WeakThis.Get();
			if (!Self || Self->FlowSerial != Serial || bForced)
			{
				return;
			}
			Self->FinishReading();
		});
	if (!Reader.IsValid())
	{
		// 界面缺失时不卡住玩家：直接放回
		FinishReading();
	}
}

void ATAWorldDocument::FinishReading()
{
	if (State != EState::Reading)
	{
		return;
	}
	Reader.Reset();
	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this))
	{
		Rollback->CommitScope(ReadingScope);
	}

	State = EState::PuttingDown;
	const uint32 Serial = ++FlowSerial;
	UTACharacterActionComponent* Action = ActionComponent.Get();
	if (!Action)
	{
		EndPutDown();
		return;
	}
	Action->PlayAction(TAGameplayTags::Anim_PutDown, [WeakThis = TWeakObjectPtr<ATAWorldDocument>(this), Serial](bool)
	{
		ATAWorldDocument* Self = WeakThis.Get();
		if (Self && Self->FlowSerial == Serial && Self->State == EState::PuttingDown)
		{
			Self->EndPutDown();
		}
	});
}

void ATAWorldDocument::EndPutDown()
{
	// 放下动画被打断或没有关键帧时也保证模型回来
	SetWorldMeshVisible(true);
	UnbindAction();
	State = EState::Idle;
}

void ATAWorldDocument::AbortReading()
{
	if (State == EState::Idle)
	{
		return;
	}
	++FlowSerial;
	if (UTADocumentReaderWidget* Open = Reader.Get())
	{
		Reader.Reset();
		Open->ForceClose();
	}
	EndPutDown();
}

void ATAWorldDocument::HandleActionKeyframe(FGameplayTag ActionTag, FName KeyName)
{
	if (ActionTag == TAGameplayTags::Anim_Pickup && KeyName == KeyHideWorldMesh && State == EState::PickingUp)
	{
		SetWorldMeshVisible(false);
	}
	else if (ActionTag == TAGameplayTags::Anim_PutDown && KeyName == KeyShowWorldMesh && State == EState::PuttingDown)
	{
		SetWorldMeshVisible(true);
	}
}

void ATAWorldDocument::SetWorldMeshVisible(bool bVisible)
{
	// 只切换实际在用的那个模型组件
	if (bUsesSkeletalMesh)
	{
		if (SkeletalMeshComponent) SkeletalMeshComponent->SetVisibility(bVisible);
	}
	else if (MeshComponent)
	{
		MeshComponent->SetVisibility(bVisible);
	}
	// 模型隐藏期间扫描既不高亮也不显示信息
	if (ScannableComponent)
	{
		ScannableComponent->bCanBeScanned = bVisible;
		ScannableComponent->SetScanHighlightEnabled(bVisible);
	}
}

void ATAWorldDocument::BindAction(UTACharacterActionComponent* InAction)
{
	UnbindAction();
	ActionComponent = InAction;
	InAction->OnActionKeyframe.AddDynamic(this, &ATAWorldDocument::HandleActionKeyframe);
}

void ATAWorldDocument::UnbindAction()
{
	if (UTACharacterActionComponent* Action = ActionComponent.Get())
	{
		Action->OnActionKeyframe.RemoveDynamic(this, &ATAWorldDocument::HandleActionKeyframe);
	}
	ActionComponent.Reset();
}

FText ATAWorldDocument::Localize(const FString& TextId) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	return Loc ? Loc->GetText(TextId) : FText::FromString(TextId);
}

