#include "TALocationRevealSetupCommandlet.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "UI/TALocationRevealWidget.h"
#include "Engine/World.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"

int32 UTALocationRevealSetupCommandlet::Main(const FString& Params)
{
 UFont* Global=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/TA_GlobalFont.TA_GlobalFont"));
 if (!Global) return 1;
 FString Source=FPaths::ProjectDir()/TEXT("Content/Fonts/GoogleSans/Source/GoogleSans-Medium.ttf");
 FParse::Value(*Params,TEXT("FontSource="),Source);
 const FString FacePath=TEXT("/Game/Fonts/GoogleSans/GoogleSans_Medium");
 UFontFace* Face=FPackageName::DoesPackageExist(FacePath) ? LoadObject<UFontFace>(nullptr,*(FacePath+TEXT(".GoogleSans_Medium"))) : nullptr;
 auto Save=[](UObject* Asset)
 {
  UPackage* Package=Asset->GetOutermost(); Package->MarkPackageDirty();
  FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
  return UPackage::SavePackage(Package,Asset,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 };
 if (!Face)
 {
  TArray<uint8> Data;
  if (!FFileHelper::LoadFileToArray(Data,*Source)) return 2;
  Face=NewObject<UFontFace>(CreatePackage(*FacePath),TEXT("GoogleSans_Medium"),RF_Public|RF_Standalone);
  Face->InitializeFromBulkData(Source,EFontHinting::Default,Data.GetData(),Data.Num());
  Face->LoadingPolicy=EFontLoadingPolicy::Inline;
  FAssetRegistryModule::AssetCreated(Face);
  if (!Save(Face)) return 3;
 }
 const FName Name(TEXT("GoogleSansMedium"));
 auto Add=[&](FTypeface& Typeface,const FFontData& Data)
 {
  for (auto& Entry:Typeface.Fonts) if (Entry.Name==Name) { Entry.Font=Data; return; }
  FTypefaceEntry Entry; Entry.Name=Name; Entry.Font=Data; Typeface.Fonts.Add(Entry);
 };
 Add(Global->CompositeFont.DefaultTypeface,FFontData(Face));
 // Existing culture/character-range routing remains authoritative for CJK. Give the new
 // named style an alias in each fallback so a missing Latin face cannot replace CJK.
 for (auto& Sub:Global->CompositeFont.SubTypefaces)
  if (!Sub.Typeface.Fonts.IsEmpty()) { const FFontData Data=Sub.Typeface.Fonts[0].Font; Add(Sub.Typeface,Data); }
 Global->CompositeFont.MakeDirty();
 if (!Save(Global)) return 4;
 const FString BPPath=TEXT("/Game/UI/LocationReveal/WBP_LocationReveal");
 if (!FPackageName::DoesPackageExist(BPPath))
 {
  UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UTALocationRevealWidget::StaticClass(),CreatePackage(*BPPath),TEXT("WBP_LocationReveal"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass());
  FKismetEditorUtilities::CompileBlueprint(BP);
  FAssetRegistryModule::AssetCreated(BP);
  if (!Save(BP)) return 5;
 }
 UE_LOG(LogTemp,Display,TEXT("Location reveal font/style and widget Blueprint ready."));
 if (FParse::Param(*Params,TEXT("Render")))
 {
  if (!FSlateApplication::IsInitialized())
   FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer());
  if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
  UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
  UClass* RuntimeClass=LoadClass<UTALocationRevealWidget>(nullptr,TEXT("/Game/UI/LocationReveal/WBP_LocationReveal.WBP_LocationReveal_C"));
  if (!RuntimeClass) return 7;
  FWidgetRenderer Renderer(true);
  struct FShot { const TCHAR* Name; float Time; FVector2D Size; };
  const FShot Shots[]={
   {TEXT("Reveal"),.4f,{1920,1080}}, {TEXT("Holding"),1.2f,{1920,1080}},
   {TEXT("Erase"),3.1f,{1920,1080}}, {TEXT("Tail"),3.85f,{1920,1080}},
   {TEXT("Holding720"),1.2f,{1280,720}}, {TEXT("Holding1440"),1.2f,{2560,1440}},
   {TEXT("Interrupted"),.4f,{1920,1080}}};
  for (const auto& Shot:Shots)
  {
   auto* Widget=CreateWidget<UTALocationRevealWidget>(World,RuntimeClass);
   const auto Slate=Widget->TakeWidget();
   Widget->ShowLocationReveal(FText::FromString(TEXT("CHAPTER 01")),FText::FromString(TEXT("HOSPITAL")));
   Widget->AdvancePresentation(Shot.Time);
   if (FString(Shot.Name)==TEXT("Interrupted"))
   {
    Widget->ShowLocationReveal(FText::FromString(TEXT("CHAPTER 02")),FText::FromString(TEXT("COURTYARD")));
    Widget->AdvancePresentation(.1f);
   }
   auto* Target=Renderer.DrawWidget(Slate,Shot.Size);
   for (int Frame=0;Frame<3;++Frame) { FlushRenderingCommands(); Renderer.DrawWidget(Target,Slate,Shot.Size,0); }
   FlushRenderingCommands();
   FBufferArchive Pixels;
   const FString Output=FPaths::ProjectSavedDir()/TEXT("LocationReveal")/(FString(Shot.Name)+TEXT(".png"));
   if (!FImageUtils::ExportRenderTarget2DAsPNG(Target,Pixels) || !FFileHelper::SaveArrayToFile(Pixels,*Output)) return 6;
   Widget->RemoveFromParent();
  }
  World->DestroyWorld(false);
 }
 return 0;
}
