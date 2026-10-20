#pragma once
#include "Commandlets/Commandlet.h"
#include "TADocumentSamplesCommandlet.generated.h"

/**
 * 生成文本系统的示例数据（已存在的资产不覆盖）：
 * /Game/Document/DA_Item_Diary（日记本，剧情槽 0）、DA_Document_Sample01、DA_Document_Sample02。
 * 用法：UnrealEditor-Cmd The_Awakening.uproject -run=TADocumentSamples
 */
UCLASS()
class UTADocumentSamplesCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTADocumentSamplesCommandlet();
	virtual int32 Main(const FString& Params) override;
};
