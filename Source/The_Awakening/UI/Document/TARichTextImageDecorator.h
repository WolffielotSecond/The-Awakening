#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockDecorator.h"
#include "TARichTextImageDecorator.generated.h"

class UTexture2D;
struct FSlateBrush;

/**
 * 文本正文插图：<img id="key"/>。key 在当前阅读文本（UTADocumentReaderWidget::GetDocument）的 InlineImages 里查。
 * 在 WBP 的 RichText 控件 Decorator Classes 中添加本类。图片按原尺寸显示，超过 MaxImageWidth 时等比缩小。
 * 建议把 <img/> 单独放一行，图片前后自然换行。找不到图片时显示 [image: key] 并打 Warning。
 */
UCLASS()
class THE_AWAKENING_API UTARichTextImageDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;

	/** 图片最大显示宽度（Slate 单位） */
	UPROPERTY(EditAnywhere, Category = "Image")
	float MaxImageWidth = 560.f;

	/** 返回的画刷由本对象持有，生命周期与 RichText 控件一致 */
	const FSlateBrush* FindImageBrush(URichTextBlock* Owner, FName ImageId);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedTextures;

	TMap<TObjectKey<UTexture2D>, TSharedPtr<FSlateBrush>> Brushes;
};
