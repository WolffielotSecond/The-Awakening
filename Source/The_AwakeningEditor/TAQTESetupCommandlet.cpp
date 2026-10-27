#include "TAQTESetupCommandlet.h"
#include "Debug/TADebugQTETester.h"
#include "Interaction/TASpecialActionDefinition.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
namespace
{
 bool SaveQTEAsset(UObject* Asset)
 {
  const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
  FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
  return UPackage::SavePackage(Asset->GetOutermost(), Asset, *File, Args);
 }
 template<typename T> T* Asset(const FString& Path)
 {
  const FString Name = FPackageName::GetShortName(Path);
  if (FPackageName::DoesPackageExist(Path)) return LoadObject<T>(nullptr, *(Path + TEXT(".") + Name));
  T* Object = NewObject<T>(CreatePackage(*Path), *Name, RF_Public | RF_Standalone);
  FAssetRegistryModule::AssetCreated(Object); return Object;
 }
 UMaterial* Material(const FString& Path, bool bKey)
 {
  auto* M = Asset<UMaterial>(Path); if (!M) return nullptr;
  M->MaterialDomain = MD_UI; M->BlendMode = BLEND_Translucent; M->TwoSided = true;
  M->GetExpressionCollection().Empty();
  auto* UV = NewObject<UMaterialExpressionTextureCoordinate>(M); M->GetExpressionCollection().AddExpression(UV);
  auto* C = NewObject<UMaterialExpressionCustom>(M); C->OutputType = CMOT_Float4; C->Inputs.Reset(); M->GetExpressionCollection().AddExpression(C);
  auto Add = [&](const TCHAR* Name, UMaterialExpression* E)
  { FCustomInput Input; Input.InputName = Name; Input.Input.Connect(0, E); C->Inputs.Add(Input); };
  auto Scalar = [&](const TCHAR* Name, float Default)
  { auto* E = NewObject<UMaterialExpressionScalarParameter>(M); E->ParameterName = Name; E->DefaultValue = Default; M->GetExpressionCollection().AddExpression(E); Add(Name,E); };
  Add(TEXT("UV"), UV);
  if (bKey)
  {
   for (const TCHAR* Name : {TEXT("FrontIcon"), TEXT("BackIcon")})
   {
    auto* T = NewObject<UMaterialExpressionTextureObjectParameter>(M); T->ParameterName = Name;
    T->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
    M->GetExpressionCollection().AddExpression(T); Add(Name,T);
   }
   Scalar(TEXT("Flip"), 0); Scalar(TEXT("FrontAspect"), 1); Scalar(TEXT("BackAspect"), 1);
   // Inverse perspective projection of a card rotating about its vertical axis.
   C->Code = TEXT("float theta=Flip*3.14159265; float c=cos(theta), s=sin(theta); float2 p=(UV-0.5)*2; float denom=c+p.x*s/3; if(abs(denom)<0.002 || abs(c)<0.002) return 0; float u=p.x/denom; float v=p.y*(1-u*s/3); float2 q=float2(u*(c>=0?1:-1),v)*0.5+0.5; float aspect=c>=0?FrontAspect:BackAspect; if(aspect>1) q.y=(q.y-0.5)*aspect+0.5; else q.x=(q.x-0.5)/max(aspect,0.01)+0.5; if(any(q<0)||any(q>1)) return 0; return c>=0?Texture2DSample(FrontIcon,FrontIconSampler,q):Texture2DSample(BackIcon,BackIconSampler,q);");
  }
  else
  {
   Scalar(TEXT("Progress"), 0);
   C->Code = TEXT("float2 p=UV-0.5; float r=length(p); float aa=max(fwidth(r),0.001); float ring=1-smoothstep(0.022-aa,0.022+aa,abs(r-0.45)); float a=frac(atan2(-p.x,p.y)/6.2831853+1); float fill=(Progress>0 && a<=Progress)?1:0; return float4(lerp(float3(0.055,0.075,0.065),float3(0.06,0.9,0.22),fill),ring*lerp(0.75,1,fill));");
  }
  auto* RGB = NewObject<UMaterialExpressionComponentMask>(M); RGB->R=true; RGB->G=true; RGB->B=true; RGB->A=false; RGB->Input.Connect(0,C); M->GetExpressionCollection().AddExpression(RGB);
  auto* Alpha = NewObject<UMaterialExpressionComponentMask>(M); Alpha->R=false; Alpha->G=false; Alpha->B=false; Alpha->A=true; Alpha->Input.Connect(0,C); M->GetExpressionCollection().AddExpression(Alpha);
  M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M->GetEditorOnlyData()->Opacity.Connect(0,Alpha);
  M->PostEditChange(); return SaveQTEAsset(M) ? M : nullptr;
 }
}
UTAQTESetupCommandlet::UTAQTESetupCommandlet()
{ IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 UTAQTESetupCommandlet::Main(const FString&)
{
 bool bOk = Material(TEXT("/Game/UI/QTE/M_QTERing"),false) && Material(TEXT("/Game/UI/QTE/M_QTEKeyFlip"),true);
 const FString Root=TEXT("/Game/Gameplay/QTE/");
 TArray<UInputAction*> Primary;
 for (const TCHAR* Name : {TEXT("IA_QTE_E"),TEXT("IA_QTE_MouseLeft"),TEXT("IA_QTE_MouseRight"),TEXT("IA_QTE_F"),TEXT("IA_QTE_Q")})
 { auto* A=Asset<UInputAction>(Root+Name); if (!A) return 1; A->ValueType=EInputActionValueType::Boolean; if(!SaveQTEAsset(A)) return 1; Primary.Add(A); }
 TArray<UInputAction*> Directions;
 for (const TCHAR* Name : {TEXT("IA_MoveForward"),TEXT("IA_MoveBackward"),TEXT("IA_MoveLeft"),TEXT("IA_MoveRight")})
 {
  const FString Path=FString(TEXT("/Game/Input/Actions/"))+Name+TEXT(".")+Name;
  auto* A=LoadObject<UInputAction>(nullptr,*Path); if(!A) return 1; Directions.Add(A);
 }
 auto* IMC=Asset<UInputMappingContext>(Root+TEXT("IMC_QTE")); if(!IMC) return 1;
 IMC->UnmapAll();
 const FKey PrimaryKeys[]={EKeys::E,EKeys::LeftMouseButton,EKeys::RightMouseButton,EKeys::F,EKeys::Q};
 for(int32 I=0;I<Primary.Num();++I)
 { IMC->MapKey(Primary[I],PrimaryKeys[I]); IMC->MapKey(Primary[I],I%2==0?EKeys::Gamepad_FaceButton_Left:EKeys::Gamepad_FaceButton_Top); }
 const FKey Letters[]={EKeys::W,EKeys::S,EKeys::A,EKeys::D};
 const FKey Arrows[]={EKeys::Up,EKeys::Down,EKeys::Left,EKeys::Right};
 const FKey Pad[]={EKeys::Gamepad_DPad_Up,EKeys::Gamepad_DPad_Down,EKeys::Gamepad_DPad_Left,EKeys::Gamepad_DPad_Right};
 for(int32 I=0;I<4;++I)
 { IMC->MapKey(Directions[I],Letters[I]); IMC->MapKey(Directions[I],Arrows[I]); IMC->MapKey(Directions[I],Pad[I]); }
 bOk &= SaveQTEAsset(IMC);
 auto Configure=[&](UTASpecialActionDefinition* D,bool bRandom)
 {
  D->KeyPulseAmplitude=.1f; D->bUseUnpausedRealTime=true; D->KeyFlipDuration=.2f; D->ExitInputBufferSeconds=.2f; D->bAlign=false; D->MappingContext=IMC; D->Stages.Reset(); D->bUseQTEPresentation=true; D->CompletionEvent=NAME_None;
  FTAQTEStage S; S.Kind=ETAQTEKind::Mash; S.Action=Primary[0]; for(auto* A:Primary) S.ActionOptions.Add(A);
  S.ProgressPerPress=0.15f; S.DecayPerSecond=0.05f; S.Timeout=0; D->Stages.Add(S);
  S=FTAQTEStage(); S.Kind=ETAQTEKind::Press; S.Action=Directions[3]; S.Timeout=0;
  if(bRandom) for(auto* A:Directions) S.ActionOptions.Add(A);
  D->Stages.Add(S); return SaveQTEAsset(D);
 };
 auto* Pipe=Asset<UTASpecialActionDefinition>(Root+TEXT("DA_Debug_QTE_Pipe"));
 auto* Random=Asset<UTASpecialActionDefinition>(Root+TEXT("DA_Debug_QTE_Random"));
 if(!Pipe || !Random) return 1;
 bOk &= Configure(Pipe,false); bOk &= Configure(Random,true);
 // Bring the previously generated chapter-one example onto the same production QTE configuration.
 if(auto* Existing=LoadObject<UTASpecialActionDefinition>(nullptr,TEXT("/Game/InteractionSamples/DA_PipeAction.DA_PipeAction")))
 {
  Existing->KeyPulseAmplitude=.1f; Existing->bUseUnpausedRealTime=true; Existing->KeyFlipDuration=.2f; Existing->ExitInputBufferSeconds=.2f; Existing->MappingContext=IMC; Existing->bUseQTEPresentation=true;
  if(Existing->Stages.Num()==4)
  {
   Existing->Stages[1]=Pipe->Stages[0]; Existing->Stages[2]=Pipe->Stages[1];
   bOk &= SaveQTEAsset(Existing);
  }
 }
 const FString BPPath=TEXT("/Game/Gameplay/QTE/Debug/TA_Debug_QTE_Tester");
 if(!FPackageName::DoesPackageExist(BPPath))
 {
  auto* BP=FKismetEditorUtilities::CreateBlueprint(ATADebugQTETester::StaticClass(),CreatePackage(*BPPath),TEXT("TA_Debug_QTE_Tester"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
  FAssetRegistryModule::AssetCreated(BP); bOk &= SaveQTEAsset(BP);
 }
 UE_LOG(LogTemp,Display,TEXT("QTE setup %s: /Game/Gameplay/QTE/Debug/TA_Debug_QTE_Tester; use TASpawnQTE in PIE."),bOk?TEXT("OK"):TEXT("FAILED"));
 return bOk?0:1;
}
