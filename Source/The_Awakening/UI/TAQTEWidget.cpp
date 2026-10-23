#include "UI/TAQTEWidget.h"
#include "Interaction/TASpecialActionComponent.h"
#include "Core/TAInputIconSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

bool UTAQTEWidget::Initialize()
{
 if (!Super::Initialize()) return false;
 SetIsFocusable(false);
 SetVisibility(ESlateVisibility::HitTestInvisible);
 if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
 auto* Size = WidgetTree->ConstructWidget<USizeBox>();
 Size->SetWidthOverride(180); Size->SetHeightOverride(180); WidgetTree->RootWidget = Size;
 auto* Overlay = WidgetTree->ConstructWidget<UOverlay>(); Size->AddChild(Overlay);
 Ring = WidgetTree->ConstructWidget<UImage>();
 Ring->SetDesiredSizeOverride(FVector2D(180));
 auto* RingSize = WidgetTree->ConstructWidget<USizeBox>(); RingSize->SetWidthOverride(180); RingSize->SetHeightOverride(180); RingSize->AddChild(Ring);
 auto* RingSlot = Overlay->AddChildToOverlay(RingSize); RingSlot->SetHorizontalAlignment(HAlign_Center); RingSlot->SetVerticalAlignment(VAlign_Center);
 Key = WidgetTree->ConstructWidget<UImage>(); Key->SetDesiredSizeOverride(FVector2D(124));
 auto* KeySize = WidgetTree->ConstructWidget<USizeBox>(); KeySize->SetWidthOverride(124); KeySize->SetHeightOverride(124); KeySize->AddChild(Key);
 auto* KeySlot = Overlay->AddChildToOverlay(KeySize); KeySlot->SetHorizontalAlignment(HAlign_Center); KeySlot->SetVerticalAlignment(VAlign_Center);
 Key->SetRenderTransformPivot(FVector2D(0.5));
 if (auto* Material = LoadObject<UMaterialInterface>(nullptr, RingMaterialPath))
 { RingMaterial = UMaterialInstanceDynamic::Create(Material, this); Ring->SetBrushFromMaterial(RingMaterial); }
 if (auto* Material = LoadObject<UMaterialInterface>(nullptr, KeyMaterialPath))
 { KeyMaterial = UMaterialInstanceDynamic::Create(Material, this); Key->SetBrushFromMaterial(KeyMaterial); }
 Ring->SetDesiredSizeOverride(FVector2D(180));
 Key->SetDesiredSizeOverride(FVector2D(124));
 return true;
}
void UTAQTEWidget::Observe(UTASpecialActionComponent* Component)
{
 if (Action.Get() == Component) return;
 if (Action.IsValid()) Action->OnUpdated.RemoveDynamic(this, &UTAQTEWidget::Refresh);
 Action = Component; ObservedStage = INDEX_NONE; bFlipping = false;
 if (Component) Component->OnUpdated.AddUniqueDynamic(this, &UTAQTEWidget::Refresh);
 Refresh();
}
void UTAQTEWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (Action.IsValid()) Action->OnUpdated.AddUniqueDynamic(this, &UTAQTEWidget::Refresh);
 Refresh();
}
void UTAQTEWidget::NativeDestruct()
{
 if (Action.IsValid()) Action->OnUpdated.RemoveDynamic(this, &UTAQTEWidget::Refresh);
 if (auto* GI = GetGameInstance()) if (auto* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
  Icons->OnInputPromptsChanged.RemoveDynamic(this, &UTAQTEWidget::Refresh);
 Super::NativeDestruct();
}
UTexture2D* UTAQTEWidget::ResolveIcon() const
{
 const auto* Component = Action.Get();
 const auto* Pawn = Component ? Cast<APawn>(Component->GetOwner()) : nullptr;
 const auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
 const auto* GI = Component ? Component->GetWorld()->GetGameInstance() : nullptr;
 const auto* Icons = GI ? GI->GetSubsystem<UTAInputIconSubsystem>() : nullptr;
 if (!Icons || !PC) return nullptr;
 const auto Keys = Icons->GetPromptKeysForAction(Component->GetPromptAction(), PC->GetLocalPlayer());
 // Prefer the typed movement letter to its arrow alias; both remain active IMC bindings.
 for (const auto& Binding : Keys)
  if (!Binding.IsGamepadKey() && Binding.GetFName().ToString().Len() == 1)
   if (auto* Texture = Icons->GetIconForKey(Binding)) return Texture;
 return Icons->GetIconForActionForPlayer(Component->GetPromptAction(), PC->GetLocalPlayer());
}
void UTAQTEWidget::SetFaces(UTexture2D* Front, UTexture2D* Back)
{
 if (!KeyMaterial) return;
 if (Front) { KeyMaterial->SetTextureParameterValue(TEXT("FrontIcon"), Front); KeyMaterial->SetScalarParameterValue(TEXT("FrontAspect"), float(Front->GetSizeX()) / FMath::Max(1, Front->GetSizeY())); }
 if (Back) { KeyMaterial->SetTextureParameterValue(TEXT("BackIcon"), Back); KeyMaterial->SetScalarParameterValue(TEXT("BackAspect"), float(Back->GetSizeX()) / FMath::Max(1, Back->GetSizeY())); }
}
void UTAQTEWidget::Refresh()
{
 const auto* Component = Action.Get();
 if (!Component || !Component->IsSpecialActionActive()) { SetRenderTranslation(FVector2D::ZeroVector); SetVisibility(ESlateVisibility::Collapsed); return; }
 if (!Key || !Ring) return;
 SetVisibility(ESlateVisibility::HitTestInvisible);
 if (auto* GI = GetGameInstance()) if (auto* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
  Icons->OnInputPromptsChanged.AddUniqueDynamic(this, &UTAQTEWidget::Refresh);
 UTexture2D* Icon = ResolveIcon();
 const int32 Stage = Component->GetStageIndex();
 if (ObservedStage != Stage)
 {
  bFlipping = ObservedStage != INDEX_NONE && DisplayedIcon && Icon && DisplayedIcon != Icon;
  FlipStartedAt = Component->GetPresentationTime(); ObservedStage = Stage;
  IncomingIcon = Icon;
  if (!bFlipping) DisplayedIcon = Icon;
  SetFaces(DisplayedIcon, Icon);
 }
 else if (IncomingIcon != Icon)
 {
  // Device changes update the faces without selecting a different action or restarting progress.
  IncomingIcon = Icon;
  if (!bFlipping) DisplayedIcon = Icon;
  SetFaces(DisplayedIcon, Icon);
 }
 Key->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
 const auto* StageConfig = Component->GetCurrentStage();
 // Shift the whole QTE inside the existing screen-space prompt anchor; never move the world component.
 SetRenderTranslation(StageConfig ? StageConfig->PromptOffset : FVector2D::ZeroVector);
 Ring->SetVisibility(StageConfig && StageConfig->Kind == ETAQTEKind::Mash ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
 if (RingMaterial) RingMaterial->SetScalarParameterValue(TEXT("Progress"), Component->GetProgress());
}
void UTAQTEWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
 Super::NativeTick(Geometry, DeltaSeconds);
 const auto* Component = Action.Get();
 if (!Component || !Component->IsSpecialActionActive() || !KeyMaterial) return;
 const auto* D = Component->GetDefinition();
 if (bFlipping)
 {
  const float T = FMath::Clamp((Component->GetPresentationTime() - FlipStartedAt) / D->KeyFlipDuration, 0.f, 1.f);
  DisplayedFlip = 0.5f - 0.5f * FMath::Cos(PI * T);
  if (T >= 1.f) { bFlipping = false; DisplayedIcon = IncomingIcon; SetFaces(DisplayedIcon, DisplayedIcon); DisplayedFlip = 0; }
 }
 else DisplayedFlip = 0;
 KeyMaterial->SetScalarParameterValue(TEXT("Flip"), DisplayedFlip);
 const auto* Stage = Component->GetCurrentStage();
 DisplayedPulse = Stage && Stage->Kind == ETAQTEKind::Mash && !bFlipping ?
  1.f + D->KeyPulseAmplitude * FMath::Sin(2.f * PI * D->KeyPulseFrequency * Component->GetPresentationTime()) : 1.f;
 Key->SetRenderScale(FVector2D(DisplayedPulse));
}
