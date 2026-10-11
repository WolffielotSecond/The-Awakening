#include "UI/TACommonDialog.h"
#include "UI/TAActionPromptWidget.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TALocalizeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"

bool FTACommonDialogDefinition::IsValid() const
{
 if (Options.IsEmpty() || Options.Num()>4 || !FMath::IsFinite(TimeoutSeconds) || TimeoutSeconds<0) return false;
 TSet<FName> Results; TSet<int32> Slots;
 for (const auto& O:Options)
 {
  if (O.ResultId.IsNone() || O.LabelTextId.IsEmpty() || O.InputSlot<1 || O.InputSlot>4 || Results.Contains(O.ResultId) || Slots.Contains(O.InputSlot)) return false;
  Results.Add(O.ResultId); Slots.Add(O.InputSlot);
 }
 return TimeoutSeconds==0 || Results.Contains(TimeoutResultId);
}
void UTACommonDialogButtonWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 if (!WidgetTree->RootWidget)
 {
  Button_Option=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("Button_Option"));
  WidgetTree->RootWidget=Button_Option;
  UClass* PromptClass=ActionPromptWidgetClass.Get();
  if (!PromptClass) PromptClass=LoadClass<UTAActionPromptWidget>(nullptr,TEXT("/Game/UI/WBP_ActionPrompt.WBP_ActionPrompt_C"));
  ActionPrompt_Option=CreateWidget<UTAActionPromptWidget>(this,PromptClass?PromptClass:UTAActionPromptWidget::StaticClass());
  Button_Option->SetContent(ActionPrompt_Option);
 }
 if (Button_Option) Button_Option->OnClicked.AddUniqueDynamic(this,&UTACommonDialogButtonWidget::Clicked);
 if (ActionPrompt_Option) ActionPrompt_Option->SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UTACommonDialogButtonWidget::Configure(const FTACommonDialogOption& O,UInputAction* Action)
{ Result=O.ResultId; if (ActionPrompt_Option) ActionPrompt_Option->ConfigureLocalizedPrompt(Action,O.LabelTextId); }
void UTACommonDialogButtonWidget::Clicked() { OnChosen.Broadcast(Result); }
UTACommonDialogWidget::UTACommonDialogWidget(const FObjectInitializer& O):Super(O) { SetIsFocusable(true); }
void UTACommonDialogWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 if (!WidgetTree->RootWidget)
 {
  auto* Root=WidgetTree->ConstructWidget<UOverlay>(); WidgetTree->RootWidget=Root;
  auto* Shade=WidgetTree->ConstructWidget<UBorder>(); Shade->SetBrushColor(FLinearColor(0,0,0,.7f)); Root->AddChildToOverlay(Shade);
  auto* Panel=WidgetTree->ConstructWidget<UBorder>(); Panel->SetBrushColor(FLinearColor(.04f,.05f,.08f,1)); Panel->SetPadding(FMargin(24));
  auto* Size=WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(640); Size->SetContent(Panel);
  auto* Center=Root->AddChildToOverlay(Size); Center->SetHorizontalAlignment(HAlign_Center); Center->SetVerticalAlignment(VAlign_Center);
  auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Panel->SetContent(Box);
  Text_Title=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("Text_Title")); Box->AddChildToVerticalBox(Text_Title)->SetPadding(FMargin(0,0,0,16));
  Text_Message=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("Text_Message")); Text_Message->SetAutoWrapText(true); Box->AddChildToVerticalBox(Text_Message)->SetPadding(FMargin(0,0,0,16));
  Text_Countdown=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("Text_Countdown")); Box->AddChildToVerticalBox(Text_Countdown);
  TObjectPtr<UTACommonDialogButtonWidget>* Slots[]={&DialogOption_1,&DialogOption_2,&DialogOption_3,&DialogOption_4};
  for (auto* ButtonSlot:Slots) { *ButtonSlot=CreateWidget<UTACommonDialogButtonWidget>(this,OptionWidgetClass?OptionWidgetClass.Get():UTACommonDialogButtonWidget::StaticClass()); Box->AddChildToVerticalBox(*ButtonSlot)->SetPadding(FMargin(0,8,0,0)); }
 }
 if (auto* GI=GetGameInstance()) if (auto* L=GI->GetSubsystem<UTALocalizeSubsystem>()) L->OnLanguageChanged.AddUniqueDynamic(this,&UTACommonDialogWidget::RefreshText);
}
bool UTACommonDialogWidget::Open(const FTACommonDialogDefinition& Definition)
{
 if (!Definition.IsValid() || Handle || bFinished) return false;
 Controller=Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()); if (!Controller.IsValid()) return false;
 Data=Definition;
 UTACommonDialogButtonWidget* Buttons[]={DialogOption_1,DialogOption_2,DialogOption_3,DialogOption_4};
 for (int32 I=0;I<4;++I)
 {
  if (!Buttons[I]) { if (Data.Options.IsValidIndex(I)) return false; continue; }
  Buttons[I]->SetVisibility(Data.Options.IsValidIndex(I)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
  if (Data.Options.IsValidIndex(I)) { Buttons[I]->Configure(Data.Options[I],Controller->GetDialogAction(Data.Options[I].InputSlot)); Buttons[I]->OnChosen.AddUniqueDynamic(this,&UTACommonDialogWidget::Choose); }
 }
 RefreshText(); AddToViewport(2000);
 FTAInputRequest R; R.Owner=this; R.Priority=FMath::Max(1000,Controller->GetInputWinner().Request.Priority+1);
 R.Allowed={ETAInputCapability::Confirm,ETAInputCapability::Cursor}; R.Presentation.InputMode=ETAInputModeRequirement::GameAndUI;
 R.Presentation.bShowCursor=true; R.Presentation.Focus=ETAInputFocusRequirement::Target; R.Presentation.FocusTarget=this;
 Handle=Controller->AcquireInputRequest(R);
 if (!Handle) { RemoveFromParent(); return false; }
 Deadline=Data.TimeoutSeconds>0?FPlatformTime::Seconds()+Data.TimeoutSeconds:0;
 return true;
}
void UTACommonDialogWidget::RefreshText()
{
 auto* GI=GetGameInstance(); auto* L=GI?GI->GetSubsystem<UTALocalizeSubsystem>():nullptr;
 auto Text=[L](const FString& Id){return L?L->GetText(Id):FText::FromString(Id);};
 if (Text_Title) Text_Title->SetText(Text(Data.TitleTextId));
 if (Text_Message) Text_Message->SetText(Text(Data.MessageTextId));
 if (Text_Countdown) Text_Countdown->SetVisibility(Data.TimeoutSeconds>0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
}
TOptional<ETAInputCapability> UTACommonDialogWidget::ResolvePlayerInput(FKey Key) const
{
 if (Controller.IsValid()) for (const auto& O:Data.Options)
  if (Controller->IsKeyMappedToAction(Key,Controller->GetDialogAction(O.InputSlot))) return ETAInputCapability::Confirm;
 return {};
}
void UTACommonDialogWidget::ExecutePlayerInput(FKey Key,ETAInputCapability C)
{
 if (C!=ETAInputCapability::Confirm || !Controller.IsValid()) return;
 for (const auto& O:Data.Options) if (Controller->IsKeyMappedToAction(Key,Controller->GetDialogAction(O.InputSlot))) { Choose(O.ResultId); return; }
}
void UTACommonDialogWidget::Choose(FName Result)
{ if (Controller.IsValid() && Controller->AllowsInputFor(Handle,this,ETAInputCapability::Confirm)) Complete(Result); }
void UTACommonDialogWidget::Complete(FName Result)
{
 if (bFinished || !Data.Options.ContainsByPredicate([Result](const auto& O){return O.ResultId==Result;})) return;
 Dismiss(); OnResult.Broadcast(Result); // Release ownership before callbacks can open another dialog.
}
void UTACommonDialogWidget::ReleaseRequest()
{ const auto Old=Handle; Handle=0; if (Old && Controller.IsValid()) Controller->ReleaseInputRequest(Old); }
void UTACommonDialogWidget::Dismiss() { bFinished=true; Deadline=0; RemoveFromParent(); }
void UTACommonDialogWidget::RemoveFromParent()
{
 ReleaseRequest(); if (auto* LP=GetOwningLocalPlayer()) if (auto* S=LP->GetSubsystem<UTACommonDialogSubsystem>()) S->Forget(this);
 Super::RemoveFromParent();
}
void UTACommonDialogWidget::NativeDestruct()
{
 ReleaseRequest();
 if (auto* GI=GetGameInstance()) if (auto* L=GI->GetSubsystem<UTALocalizeSubsystem>()) L->OnLanguageChanged.RemoveDynamic(this,&UTACommonDialogWidget::RefreshText);
 if (auto* LP=GetOwningLocalPlayer()) if (auto* S=LP->GetSubsystem<UTACommonDialogSubsystem>()) S->Forget(this);
 Super::NativeDestruct();
}
void UTACommonDialogWidget::NativeTick(const FGeometry& G,float DT)
{
 Super::NativeTick(G,DT);
 if (!Deadline || bFinished) return;
 const double Remaining=Deadline-FPlatformTime::Seconds();
 if (Text_Countdown) Text_Countdown->SetText(FText::AsNumber(FMath::Max(0,FMath::CeilToInt(Remaining))));
 if (Remaining<=0) Complete(Data.TimeoutResultId);
}
UTACommonDialogWidget* UTACommonDialogSubsystem::ShowDialog(const FTACommonDialogDefinition& D)
{
 if (Active || !D.IsValid()) return nullptr;
 auto* PC=Cast<AThe_AwakeningPlayerController>(GetLocalPlayer()->GetPlayerController(GetWorld())); if (!PC) return nullptr;
 Active=CreateWidget<UTACommonDialogWidget>(PC,PC->GetCommonDialogWidgetClass());
 auto* Widget=Active.Get(); if (!Widget || !Widget->Open(D)) { if (Widget) Widget->Dismiss(); Active=nullptr; return nullptr; }
 return Widget;
}
void UTACommonDialogSubsystem::Forget(UTACommonDialogWidget* Widget) { if (Active==Widget) Active=nullptr; }
void UTACommonDialogSubsystem::Deinitialize() { if (Active) Active->Dismiss(); Super::Deinitialize(); }
