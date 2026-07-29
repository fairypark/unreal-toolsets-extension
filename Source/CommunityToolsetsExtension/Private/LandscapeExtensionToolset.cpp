#include "LandscapeExtensionToolset.h"

#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeFileFormatInterface.h"
#include "LandscapeImportHelper.h"
#include "LandscapeInfo.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace LandscapeExtensionToolsetPrivate
{
	constexpr int64 MaxExactVertexCount = 16ll * 1024ll * 1024ll;

	void RaiseError(const TCHAR* Code, const FString& Detail)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("[%s] %s"), Code, *Detail));
	}

	bool ResolveLandscape(
		ALandscapeProxy* Input,
		ULandscapeInfo*& OutInfo,
		ALandscape*& OutLandscape,
		FString& OutError)
	{
		if (!IsValid(Input))
		{
			OutError = TEXT("Landscape must reference a loaded Landscape actor or streaming proxy.");
			return false;
		}

		UWorld* World = Input->GetWorld();
		if (!IsValid(World) || World->WorldType != EWorldType::Editor)
		{
			OutError = TEXT("Landscape mutations and Editor inspection require an Editor world and are unavailable in PIE.");
			return false;
		}

		OutInfo = Input->GetLandscapeInfo();
		OutLandscape = Input->GetLandscapeActor();
		if (!IsValid(OutInfo) || !IsValid(OutLandscape) || OutLandscape->GetWorld() != World)
		{
			OutError = TEXT("The Landscape target could not be resolved to one canonical Landscape in the same world.");
			return false;
		}

		return true;
	}

	bool ResolveExtent(
		ULandscapeInfo* Info,
		const FLandscapeExtensionRegion& Region,
		FIntRect& OutExtent,
		FString& OutError)
	{
		FIntRect FullExtent;
		if (!Info->GetLandscapeExtent(FullExtent))
		{
			OutError = TEXT("The Landscape has no loaded vertex extent.");
			return false;
		}

		if (Region.bUseFullExtent)
		{
			OutExtent = FullExtent;
		}
		else
		{
			OutExtent = FIntRect(Region.MinVertex, Region.MaxVertex);
			if (OutExtent.Min.X > OutExtent.Max.X || OutExtent.Min.Y > OutExtent.Max.Y)
			{
				OutError = TEXT("Region minimum vertices must not exceed maximum vertices.");
				return false;
			}
			if (!FullExtent.Contains(OutExtent.Min) || !FullExtent.Contains(OutExtent.Max))
			{
				OutError = FString::Printf(
					TEXT("Requested region (%d,%d)-(%d,%d) is outside loaded Landscape extent (%d,%d)-(%d,%d)."),
					OutExtent.Min.X,
					OutExtent.Min.Y,
					OutExtent.Max.X,
					OutExtent.Max.Y,
					FullExtent.Min.X,
					FullExtent.Min.Y,
					FullExtent.Max.X,
					FullExtent.Max.Y);
				return false;
			}
		}

		const int64 Width = static_cast<int64>(OutExtent.Width()) + 1;
		const int64 Height = static_cast<int64>(OutExtent.Height()) + 1;
		if (Width <= 0 || Height <= 0 || Width * Height > MaxExactVertexCount)
		{
			OutError = FString::Printf(
				TEXT("Exact operations are limited to %lld vertices; requested %lld."),
				MaxExactVertexCount,
				Width * Height);
			return false;
		}

		if (Info->HasUnloadedComponentsInRegion(
			OutExtent.Min.X,
			OutExtent.Min.Y,
			OutExtent.Max.X,
			OutExtent.Max.Y))
		{
			OutError = TEXT("Every Landscape component intersecting the requested region must be loaded.");
			return false;
		}

		return true;
	}

	bool ParseEditLayerGuid(
		ALandscape* Landscape,
		const FString& Text,
		bool bRequired,
		FGuid& OutGuid,
		FString& OutError)
	{
		if (Text.IsEmpty())
		{
			if (bRequired)
			{
				OutError = TEXT("DestinationEditLayerGuid is required for mutation.");
				return false;
			}
			OutGuid.Invalidate();
			return true;
		}

		if (!FGuid::Parse(Text, OutGuid))
		{
			OutError = FString::Printf(TEXT("'%s' is not a valid edit-layer GUID."), *Text);
			return false;
		}

		const FLandscapeLayer* Layer = Landscape->GetLayerConst(OutGuid);
		if (Layer == nullptr || !IsValid(Layer->EditLayer))
		{
			OutError = FString::Printf(TEXT("Edit layer %s does not exist on the target Landscape."), *Text);
			return false;
		}

		if (bRequired && (Layer->EditLayer->IsLocked() || !Layer->EditLayer->SupportsEditingTools()))
		{
			OutError = FString::Printf(TEXT("Edit layer %s is locked or does not support editing tools."), *Text);
			return false;
		}

		return true;
	}

	TArray<FIntPoint> GetRepresentativeVertices(const FIntRect& Extent)
	{
		const int32 MidX = Extent.Min.X + Extent.Width() / 2;
		const int32 MidY = Extent.Min.Y + Extent.Height() / 2;
		TArray<FIntPoint> Points = {
			Extent.Min,
			FIntPoint(Extent.Max.X, Extent.Min.Y),
			Extent.Max,
			FIntPoint(Extent.Min.X, Extent.Max.Y),
			FIntPoint(MidX, Extent.Min.Y),
			FIntPoint(Extent.Max.X, MidY),
			FIntPoint(MidX, Extent.Max.Y),
			FIntPoint(Extent.Min.X, MidY),
			FIntPoint(MidX, MidY)
		};

		TArray<FIntPoint> UniquePoints;
		for (const FIntPoint& Point : Points)
		{
			UniquePoints.AddUnique(Point);
		}
		return UniquePoints;
	}

	double RawHeightToWorldZ(ALandscape* Landscape, const FIntPoint& Vertex, uint16 RawHeight)
	{
		const FVector LocalPosition(
			static_cast<double>(Vertex.X),
			static_cast<double>(Vertex.Y),
			LandscapeDataAccess::GetLocalHeight(RawHeight));
		return Landscape->LandscapeActorToWorld().TransformPosition(LocalPosition).Z;
	}

	bool ReadHeightData(
		ULandscapeInfo* Info,
		const FGuid& EditLayerGuid,
		const FIntRect& Extent,
		TArray<uint16>& OutData)
	{
		const int32 Width = Extent.Width() + 1;
		const int32 Height = Extent.Height() + 1;
		OutData.SetNumUninitialized(Width * Height);
		FLandscapeEditDataInterface Edit(Info, EditLayerGuid, false);
		Edit.GetHeightDataFast(
			Extent.Min.X,
			Extent.Min.Y,
			Extent.Max.X,
			Extent.Max.Y,
			OutData.GetData(),
			Width);
		return true;
	}

	void WriteHeightData(
		ULandscapeInfo* Info,
		const FGuid& EditLayerGuid,
		const FIntRect& Extent,
		const TArray<uint16>& Data)
	{
		const int32 Width = Extent.Width() + 1;
		FLandscapeEditDataInterface Edit(Info, EditLayerGuid, true);
		Edit.SetHeightData(
			Extent.Min.X,
			Extent.Min.Y,
			Extent.Max.X,
			Extent.Max.Y,
			Data.GetData(),
			Width,
			true,
			nullptr,
			nullptr,
			nullptr,
			false,
			nullptr,
			nullptr,
			true,
			true,
			true);
		Edit.Flush();

		Info->ForAllLandscapeComponents([](ULandscapeComponent* Component)
		{
			if (IsValid(Component))
			{
				Component->RequestHeightmapUpdate(true, true, true);
			}
		});
	}

	bool ReadWeightData(
		ULandscapeInfo* Info,
		const FGuid& EditLayerGuid,
		ULandscapeLayerInfoObject* LayerInfo,
		const FIntRect& Extent,
		TArray<uint8>& OutData)
	{
		const int32 Width = Extent.Width() + 1;
		const int32 Height = Extent.Height() + 1;
		OutData.SetNumUninitialized(Width * Height);
		FLandscapeEditDataInterface Edit(Info, EditLayerGuid, false);
		Edit.GetWeightDataFast(
			LayerInfo,
			Extent.Min.X,
			Extent.Min.Y,
			Extent.Max.X,
			Extent.Max.Y,
			OutData.GetData(),
			Width);
		return true;
	}

	void WriteWeightData(
		ULandscapeInfo* Info,
		const FGuid& EditLayerGuid,
		ULandscapeLayerInfoObject* LayerInfo,
		const FIntRect& Extent,
		const TArray<uint8>& Data)
	{
		const int32 Width = Extent.Width() + 1;
		FLandscapeEditDataInterface Edit(Info, EditLayerGuid, true);
		Edit.SetAlphaData(
			LayerInfo,
			Extent.Min.X,
			Extent.Min.Y,
			Extent.Max.X,
			Extent.Max.Y,
			Data.GetData(),
			Width,
			ELandscapeLayerPaintingRestriction::None);
		Edit.Flush();

		Info->ForAllLandscapeComponents([](ULandscapeComponent* Component)
		{
			if (IsValid(Component))
			{
				Component->RequestWeightmapUpdate(true, true, true);
			}
		});
	}

	void GatherLandscapePackages(ULandscapeInfo* Info, TArray<UPackage*>& OutPackages)
	{
		auto AddObjectPackage = [&OutPackages](UObject* Object)
		{
			if (IsValid(Object) && Object->GetPackage() != GetTransientPackage())
			{
				OutPackages.AddUnique(Object->GetPackage());
			}
		};

		Info->ForEachLandscapeProxy([&AddObjectPackage](ALandscapeProxy* Proxy)
		{
			AddObjectPackage(Proxy);
			return true;
		});

		Info->ForAllLandscapeComponents([&AddObjectPackage](ULandscapeComponent* Component)
		{
			AddObjectPackage(Component);
			AddObjectPackage(Component->GetHeightmap());
			for (UTexture2D* Texture : Component->GetWeightmapTextures())
			{
				AddObjectPackage(Texture);
			}
		});

		if (UWorld* World = Info->GetWorld())
		{
			AddObjectPackage(World);
		}
	}

	TArray<FString> GetPackagePaths(const TArray<UPackage*>& Packages)
	{
		TArray<FString> Paths;
		for (const UPackage* Package : Packages)
		{
			if (IsValid(Package))
			{
				Paths.AddUnique(Package->GetName());
			}
		}
		Paths.Sort();
		return Paths;
	}

	bool SavePackages(
		const TArray<UPackage*>& Packages,
		FLandscapeExtensionSaveResult& OutResult,
		FString& OutError)
	{
		TArray<UPackage*> DirtyPackages;
		for (UPackage* Package : Packages)
		{
			if (!IsValid(Package))
			{
				continue;
			}
			if (Package->IsDirty())
			{
				DirtyPackages.AddUnique(Package);
				OutResult.SavedPackagePaths.Add(Package->GetName());
			}
			else
			{
				OutResult.UnchangedPackagePaths.Add(Package->GetName());
			}
		}
		OutResult.SavedPackagePaths.Sort();
		OutResult.UnchangedPackagePaths.Sort();

		if (DirtyPackages.IsEmpty())
		{
			return true;
		}

		if (!UEditorLoadingAndSavingUtils::SavePackages(DirtyPackages, true))
		{
			OutError = FString::Printf(
				TEXT("Failed to save Landscape packages: %s"),
				*FString::Join(OutResult.SavedPackagePaths, TEXT(", ")));
			return false;
		}

		for (const UPackage* Package : DirtyPackages)
		{
			if (Package->IsDirty())
			{
				OutError = FString::Printf(TEXT("Package remained dirty after save: %s"), *Package->GetName());
				return false;
			}
		}
		return true;
	}

	bool SaveRecoveryPoint(
		ULandscapeInfo* Info,
		TArray<UPackage*>& OutPackages,
		TArray<FString>& OutPackagePaths,
		FString& OutError)
	{
		GatherLandscapePackages(Info, OutPackages);
		OutPackagePaths = GetPackagePaths(OutPackages);
		FLandscapeExtensionSaveResult IgnoredResult;
		return SavePackages(OutPackages, IgnoredResult, OutError);
	}

	FLandscapeExtensionHeightStatistics BuildStatistics(
		ALandscape* Landscape,
		const FIntRect& Extent,
		const TArray<uint16>& Data)
	{
		FLandscapeExtensionHeightStatistics Result;
		Result.MinVertex = Extent.Min;
		Result.MaxVertex = Extent.Max;
		Result.SampleCount = Data.Num();
		Result.bFullRequestedRegionCovered = true;
		if (Data.IsEmpty())
		{
			return Result;
		}

		const int32 Width = Extent.Width() + 1;
		uint16 MinRaw = TNumericLimits<uint16>::Max();
		uint16 MaxRaw = TNumericLimits<uint16>::Min();
		double Sum = 0.0;
		double SumSquares = 0.0;
		double MinWorld = TNumericLimits<double>::Max();
		double MaxWorld = TNumericLimits<double>::Lowest();

		for (int32 Index = 0; Index < Data.Num(); ++Index)
		{
			const int32 X = Extent.Min.X + Index % Width;
			const int32 Y = Extent.Min.Y + Index / Width;
			const uint16 Raw = Data[Index];
			const double WorldZ = RawHeightToWorldZ(Landscape, FIntPoint(X, Y), Raw);
			MinRaw = FMath::Min(MinRaw, Raw);
			MaxRaw = FMath::Max(MaxRaw, Raw);
			MinWorld = FMath::Min(MinWorld, WorldZ);
			MaxWorld = FMath::Max(MaxWorld, WorldZ);
			Sum += WorldZ;
			SumSquares += WorldZ * WorldZ;
		}

		Result.MinRawHeight = MinRaw;
		Result.MaxRawHeight = MaxRaw;
		Result.MinWorldZCentimeters = MinWorld;
		Result.MaxWorldZCentimeters = MaxWorld;
		Result.MeanWorldZCentimeters = Sum / Data.Num();
		Result.StandardDeviationCentimeters = FMath::Sqrt(FMath::Max(
			0.0,
			SumSquares / Data.Num() - Result.MeanWorldZCentimeters * Result.MeanWorldZCentimeters));
		Result.RangeCentimeters = MaxWorld - MinWorld;

		for (const FIntPoint& Point : GetRepresentativeVertices(Extent))
		{
			const int32 Index = (Point.Y - Extent.Min.Y) * Width + (Point.X - Extent.Min.X);
			FLandscapeExtensionHeightSample Sample;
			Sample.Vertex = Point;
			Sample.RawHeight = Data[Index];
			Sample.WorldZCentimeters = RawHeightToWorldZ(Landscape, Point, Data[Index]);
			Result.RepresentativeSamples.Add(Sample);
		}

		return Result;
	}

	bool DecodeHeightmap(
		const FString& SourceFile,
		bool bFlipYAxis,
		const FIntRect& Extent,
		TArray<uint16>& OutData,
		TArray<FString>& OutWarnings,
		FString& OutResolvedSource,
		FString& OutError)
	{
		if (SourceFile.IsEmpty())
		{
			OutError = TEXT("SourceFile is required.");
			return false;
		}

		OutResolvedSource = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), SourceFile);
		FPaths::NormalizeFilename(OutResolvedSource);
		if (!IFileManager::Get().FileExists(*OutResolvedSource))
		{
			OutError = FString::Printf(TEXT("Heightmap source does not exist: %s"), *OutResolvedSource);
			return false;
		}

		FLandscapeImportDescriptor Descriptor;
		FText Message;
		const ELandscapeImportResult DescriptorResult =
			FLandscapeImportHelper::GetHeightmapImportDescriptor(
				OutResolvedSource,
				true,
				bFlipYAxis,
				Descriptor,
				Message);
		if (DescriptorResult == ELandscapeImportResult::Error)
		{
			OutError = Message.ToString();
			return false;
		}
		if (DescriptorResult == ELandscapeImportResult::Warning)
		{
			OutWarnings.Add(Message.ToString());
		}

		const int32 Width = Extent.Width() + 1;
		const int32 Height = Extent.Height() + 1;
		const int32 DescriptorIndex = Descriptor.FindDescriptorIndex(Width, Height);
		if (DescriptorIndex == INDEX_NONE)
		{
			TArray<FString> Resolutions;
			for (const FLandscapeImportResolution& Resolution : Descriptor.ImportResolutions)
			{
				Resolutions.Add(FString::Printf(TEXT("%ux%u"), Resolution.Width, Resolution.Height));
			}
			OutError = FString::Printf(
				TEXT("Heightmap resolution must be exactly %dx%d; source offers: %s"),
				Width,
				Height,
				*FString::Join(Resolutions, TEXT(", ")));
			return false;
		}

		const ELandscapeImportResult DataResult =
			FLandscapeImportHelper::GetHeightmapImportData(Descriptor, DescriptorIndex, OutData, Message);
		if (DataResult == ELandscapeImportResult::Error || OutData.Num() != Width * Height)
		{
			OutError = DataResult == ELandscapeImportResult::Error
				? Message.ToString()
				: TEXT("Decoded heightmap size does not match its validated descriptor.");
			return false;
		}
		if (DataResult == ELandscapeImportResult::Warning)
		{
			OutWarnings.Add(Message.ToString());
		}

		return true;
	}

	bool DecodeWeightmap(
		const FString& SourceFile,
		bool bFlipYAxis,
		FName LayerName,
		const FIntRect& Extent,
		TArray<uint8>& OutData,
		TArray<FString>& OutWarnings,
		FString& OutResolvedSource,
		FString& OutError)
	{
		if (SourceFile.IsEmpty())
		{
			OutError = TEXT("SourceFile is required.");
			return false;
		}

		OutResolvedSource = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), SourceFile);
		FPaths::NormalizeFilename(OutResolvedSource);
		if (!IFileManager::Get().FileExists(*OutResolvedSource))
		{
			OutError = FString::Printf(TEXT("Weightmap source does not exist: %s"), *OutResolvedSource);
			return false;
		}

		FLandscapeImportDescriptor Descriptor;
		FText Message;
		const ELandscapeImportResult DescriptorResult =
			FLandscapeImportHelper::GetWeightmapImportDescriptor(
				OutResolvedSource,
				true,
				bFlipYAxis,
				LayerName,
				Descriptor,
				Message);
		if (DescriptorResult == ELandscapeImportResult::Error)
		{
			OutError = Message.ToString();
			return false;
		}
		if (DescriptorResult == ELandscapeImportResult::Warning)
		{
			OutWarnings.Add(Message.ToString());
		}

		const int32 Width = Extent.Width() + 1;
		const int32 Height = Extent.Height() + 1;
		const int32 DescriptorIndex = Descriptor.FindDescriptorIndex(Width, Height);
		if (DescriptorIndex == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Weightmap resolution must be exactly %dx%d."), Width, Height);
			return false;
		}

		const ELandscapeImportResult DataResult =
			FLandscapeImportHelper::GetWeightmapImportData(
				Descriptor,
				DescriptorIndex,
				LayerName,
				OutData,
				Message);
		if (DataResult == ELandscapeImportResult::Error || OutData.Num() != Width * Height)
		{
			OutError = DataResult == ELandscapeImportResult::Error
				? Message.ToString()
				: TEXT("Decoded weightmap size does not match its validated descriptor.");
			return false;
		}
		if (DataResult == ELandscapeImportResult::Warning)
		{
			OutWarnings.Add(Message.ToString());
		}

		return true;
	}

	void ModifyLandscapeObjects(ULandscapeInfo* Info)
	{
		Info->ForEachLandscapeProxy([](ALandscapeProxy* Proxy)
		{
			if (IsValid(Proxy))
			{
				Proxy->Modify();
			}
			return true;
		});
		Info->ForAllLandscapeComponents([](ULandscapeComponent* Component)
		{
			if (IsValid(Component))
			{
				Component->Modify();
			}
		});
	}

	bool VerifyExact(const TArray<uint16>& Expected, const TArray<uint16>& Actual)
	{
		return Expected.Num() == Actual.Num()
			&& FMemory::Memcmp(
				Expected.GetData(),
				Actual.GetData(),
				Expected.Num() * sizeof(uint16)) == 0;
	}

	bool VerifyExact(const TArray<uint8>& Expected, const TArray<uint8>& Actual)
	{
		return Expected == Actual;
	}

	FLandscapeExtensionMutationResult ApplyHeightmap(
		ALandscapeProxy* InputLandscape,
		const FLandscapeExtensionHeightmapRequest& Request,
		bool bReimport)
	{
		FLandscapeExtensionMutationResult Result;
		Result.Operation = bReimport ? TEXT("ReimportHeightmap") : TEXT("ImportHeightmap");

		ULandscapeInfo* Info = nullptr;
		ALandscape* Landscape = nullptr;
		FString Error;
		if (!ResolveLandscape(InputLandscape, Info, Landscape, Error))
		{
			RaiseError(TEXT("LANDSCAPE_EXTENSION_TARGET_INVALID"), Error);
			return {};
		}

		FLandscapeExtensionRegion FullRegion;
		FIntRect Extent;
		if (!ResolveExtent(Info, FullRegion, Extent, Error))
		{
			RaiseError(TEXT("LANDSCAPE_EXTENSION_WORLD_PARTITION_PARTIAL"), Error);
			return {};
		}

		FGuid EditLayerGuid;
		if (!ParseEditLayerGuid(Landscape, Request.DestinationEditLayerGuid, true, EditLayerGuid, Error))
		{
			RaiseError(TEXT("LANDSCAPE_EXTENSION_DESTINATION_LAYER_INVALID"), Error);
			return {};
		}

		FString RequestedSource = Request.SourceFile;
		if (bReimport)
		{
			if (Landscape->ReimportHeightmapFilePath.IsEmpty())
			{
				RaiseError(
					TEXT("LANDSCAPE_EXTENSION_REIMPORT_SOURCE_MISSING"),
					TEXT("The target Landscape has no recorded heightmap source. Use ImportHeightmap first."));
				return {};
			}
			if (RequestedSource.IsEmpty())
			{
				RequestedSource = Landscape->ReimportHeightmapFilePath;
			}
		}

		TArray<uint16> ImportedData;
		FString ResolvedSource;
		if (!DecodeHeightmap(
			RequestedSource,
			Request.bFlipYAxis,
			Extent,
			ImportedData,
			Result.Warnings,
			ResolvedSource,
			Error))
		{
			RaiseError(TEXT("LANDSCAPE_EXTENSION_SOURCE_INVALID"), Error);
			return {};
		}

		if (bReimport)
		{
			FString ExistingSource = FPaths::ConvertRelativePathToFull(
				FPaths::ProjectDir(),
				Landscape->ReimportHeightmapFilePath);
			FPaths::NormalizeFilename(ExistingSource);
			if (!ExistingSource.Equals(ResolvedSource, ESearchCase::IgnoreCase))
			{
				RaiseError(
					TEXT("LANDSCAPE_EXTENSION_REIMPORT_SOURCE_MISMATCH"),
					TEXT("ReimportHeightmap only accepts the Landscape's recorded source. Use ImportHeightmap to replace it."));
				return {};
			}
		}

		TArray<UPackage*> Packages;
		if (!SaveRecoveryPoint(Info, Packages, Result.RecoveryPackagePaths, Error))
		{
			RaiseError(TEXT("LANDSCAPE_EXTENSION_RECOVERY_SAVE_FAILED"), Error);
			return {};
		}

		TArray<uint16> OriginalData;
		ReadHeightData(Info, EditLayerGuid, Extent, OriginalData);
		Result.BeforeHeight = BuildStatistics(Landscape, Extent, OriginalData);

		{
			const FScopedTransaction Transaction(
				FText::FromString(FString::Printf(TEXT("Landscape Extension %s"), *Result.Operation)));
			ModifyLandscapeObjects(Info);
			WriteHeightData(Info, EditLayerGuid, Extent, ImportedData);
			Landscape->ReimportHeightmapFilePath = ResolvedSource;
			Landscape->MarkPackageDirty();
		}

		TArray<uint16> VerifiedData;
		ReadHeightData(Info, EditLayerGuid, Extent, VerifiedData);
		if (!VerifyExact(ImportedData, VerifiedData))
		{
			WriteHeightData(Info, EditLayerGuid, Extent, OriginalData);
			TArray<uint16> RollbackData;
			ReadHeightData(Info, EditLayerGuid, Extent, RollbackData);
			const bool bRollbackVerified = VerifyExact(OriginalData, RollbackData);
			RaiseError(
				bRollbackVerified
					? TEXT("LANDSCAPE_EXTENSION_POST_VERIFY_FAILED_ROLLED_BACK")
					: TEXT("LANDSCAPE_EXTENSION_POST_VERIFY_FAILED_RELOAD_REQUIRED"),
				bRollbackVerified
					? TEXT("Imported heights did not verify and the original heights were restored.")
					: FString::Printf(
						TEXT("Height verification and in-memory rollback failed. Reload packages: %s"),
						*FString::Join(Result.RecoveryPackagePaths, TEXT(", "))));
			return {};
		}

		Result.Landscape = Landscape;
		Result.SourceFile = ResolvedSource;
		Result.AfterHeight = BuildStatistics(Landscape, Extent, VerifiedData);
		Result.ModifiedPackagePaths = GetPackagePaths(Packages);

		if (Request.bSaveAfterSuccess && !SavePackages(Packages, Result.SaveResult, Error))
		{
			WriteHeightData(Info, EditLayerGuid, Extent, OriginalData);
			FLandscapeExtensionSaveResult RollbackSave;
			FString RollbackError;
			const bool bRollbackSaved = SavePackages(Packages, RollbackSave, RollbackError);
			RaiseError(
				bRollbackSaved
					? TEXT("LANDSCAPE_EXTENSION_POST_SAVE_FAILED_ROLLED_BACK")
					: TEXT("LANDSCAPE_EXTENSION_POST_SAVE_FAILED_RELOAD_REQUIRED"),
				bRollbackSaved
					? Error
					: FString::Printf(
						TEXT("%s Rollback save also failed: %s"),
						*Error,
						*RollbackError));
			return {};
		}

		return Result;
	}
}

FLandscapeExtensionInfoResult ULandscapeExtensionToolset::GetLandscapeInfo(ALandscapeProxy* Landscape)
{
	using namespace LandscapeExtensionToolsetPrivate;
	FLandscapeExtensionInfoResult Result;
	ULandscapeInfo* Info = nullptr;
	ALandscape* MainLandscape = nullptr;
	FString Error;
	if (!ResolveLandscape(Landscape, Info, MainLandscape, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_TARGET_INVALID"), Error);
		return {};
	}

	FIntRect Extent;
	if (!Info->GetLandscapeExtent(Extent))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_EXTENT_UNAVAILABLE"), TEXT("The Landscape has no loaded vertex extent."));
		return {};
	}

	Result.Landscape = MainLandscape;
	Result.LandscapeGuid = Info->LandscapeGuid.ToString(EGuidFormats::DigitsWithHyphens);
	Result.WorldPath = MainLandscape->GetWorld()->GetPathName();
	Result.Transform = MainLandscape->LandscapeActorToWorld();
	Result.MinVertex = Extent.Min;
	Result.MaxVertex = Extent.Max;
	Result.VertexResolution = FIntPoint(Extent.Width() + 1, Extent.Height() + 1);
	Result.bWorldPartition = MainLandscape->GetWorld()->IsPartitionedWorld();
	Result.bHasUnloadedComponents = Info->HasUnloadedComponentsInRegion(
		Extent.Min.X,
		Extent.Min.Y,
		Extent.Max.X,
		Extent.Max.Y);
	Result.ReimportHeightmapFilePath = MainLandscape->ReimportHeightmapFilePath;
	if (UMaterialInterface* Material = MainLandscape->GetLandscapeMaterial())
	{
		Result.MaterialPath = Material->GetPathName();
	}

	FBox Bounds(ForceInit);
	Info->ForAllLandscapeComponents([&Result, &Bounds](ULandscapeComponent* Component)
	{
		if (IsValid(Component))
		{
			++Result.LoadedComponentCount;
			Bounds += Component->Bounds.GetBox();
		}
	});
	for (const TWeakObjectPtr<ALandscapeStreamingProxy>& Proxy : Info->GetSortedStreamingProxies())
	{
		if (Proxy.IsValid())
		{
			++Result.LoadedStreamingProxyCount;
		}
	}
	if (Bounds.IsValid)
	{
		Result.BoundsMinCentimeters = Bounds.Min;
		Result.BoundsMaxCentimeters = Bounds.Max;
	}

	for (const FLandscapeLayer& Layer : MainLandscape->GetLayersConst())
	{
		if (IsValid(Layer.EditLayer))
		{
			FLandscapeExtensionEditLayerInfo LayerResult;
			LayerResult.Name = Layer.EditLayer->GetName().ToString();
			LayerResult.Guid = Layer.EditLayer->GetGuid().ToString(EGuidFormats::DigitsWithHyphens);
			Result.EditLayers.Add(LayerResult);
		}
	}

	TArray<UPackage*> Packages;
	GatherLandscapePackages(Info, Packages);
	for (const UPackage* Package : Packages)
	{
		if (IsValid(Package) && Package->IsDirty())
		{
			Result.DirtyPackagePaths.Add(Package->GetName());
		}
	}
	Result.DirtyPackagePaths.Sort();
	return Result;
}

FLandscapeExtensionMutationResult ULandscapeExtensionToolset::CreateLandscape(
	UWorld* World,
	const FLandscapeExtensionCreateRequest& Request)
{
	using namespace LandscapeExtensionToolsetPrivate;
	FLandscapeExtensionMutationResult Result;
	Result.Operation = TEXT("CreateLandscape");

	if (!IsValid(World) || World->WorldType != EWorldType::Editor)
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_WORLD_INVALID"), TEXT("CreateLandscape requires an Editor world."));
		return {};
	}
	if (World->IsPartitionedWorld())
	{
		RaiseError(
			TEXT("LANDSCAPE_EXTENSION_WORLD_PARTITION_CREATE_UNSUPPORTED"),
			TEXT("Version 0.1 does not create World Partition streaming proxies. Create in a non-partitioned world or use a future compatibility implementation."));
		return {};
	}
	if (Request.SectionsPerComponent < 1 || Request.SectionsPerComponent > 2)
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_LAYOUT_INVALID"), TEXT("SectionsPerComponent must be 1 or 2."));
		return {};
	}
	const TArray<int32> ValidQuadsPerSection = {7, 15, 31, 63, 127, 255};
	if (!ValidQuadsPerSection.Contains(Request.QuadsPerSection)
		|| Request.QuadsPerSection * Request.SectionsPerComponent > 255
		|| Request.ComponentCountX < 1
		|| Request.ComponentCountY < 1)
	{
		RaiseError(
			TEXT("LANDSCAPE_EXTENSION_LAYOUT_INVALID"),
			TEXT("Use valid Landscape quads per section, positive component counts, and no more than 255 quads per component."));
		return {};
	}
	if (Request.FlatRawHeight < 0 || Request.FlatRawHeight > 65535)
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_HEIGHT_INVALID"), TEXT("FlatRawHeight must be in the uint16 range."));
		return {};
	}

	const int32 QuadsPerComponent = Request.QuadsPerSection * Request.SectionsPerComponent;
	const int32 SizeX = Request.ComponentCountX * QuadsPerComponent + 1;
	const int32 SizeY = Request.ComponentCountY * QuadsPerComponent + 1;
	const int64 VertexCount = static_cast<int64>(SizeX) * SizeY;
	if (VertexCount > MaxExactVertexCount)
	{
		RaiseError(
			TEXT("LANDSCAPE_EXTENSION_LAYOUT_TOO_LARGE"),
			FString::Printf(TEXT("Version 0.1 limits creation to %lld vertices."), MaxExactVertexCount));
		return {};
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Landscape Extension Create Landscape")));
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Request.Location, Request.Rotation);
	if (!IsValid(Landscape))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_SPAWN_FAILED"), TEXT("The Editor failed to spawn a Landscape actor."));
		return {};
	}

	Landscape->SetActorScale3D(Request.Scale);
	Landscape->SetActorLabel(Request.ActorLabel.IsEmpty() ? TEXT("Landscape") : Request.ActorLabel);
	if (IsValid(Request.Material))
	{
		Landscape->LandscapeMaterial = Request.Material;
	}

	TArray<uint16> HeightData;
	HeightData.Init(static_cast<uint16>(Request.FlatRawHeight), VertexCount);
	TMap<FGuid, TArray<uint16>> HeightDataPerLayer;
	HeightDataPerLayer.Add(FGuid(), MoveTemp(HeightData));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialDataPerLayer;
	MaterialDataPerLayer.Add(FGuid(), {});

	Landscape->Import(
		FGuid::NewGuid(),
		0,
		0,
		SizeX - 1,
		SizeY - 1,
		Request.SectionsPerComponent,
		Request.QuadsPerSection,
		HeightDataPerLayer,
		TEXT(""),
		MaterialDataPerLayer,
		ELandscapeImportAlphamapType::Additive,
		TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	FIntRect Extent;
	if (!IsValid(Info)
		|| !Info->GetLandscapeExtent(Extent)
		|| Extent.Width() + 1 != SizeX
		|| Extent.Height() + 1 != SizeY)
	{
		World->DestroyActor(Landscape);
		RaiseError(
			TEXT("LANDSCAPE_EXTENSION_CREATE_VERIFY_FAILED"),
			TEXT("The created Landscape did not report the requested resolution and was removed."));
		return {};
	}

	Info->UpdateLayerInfoMap(Landscape);
	Result.Landscape = Landscape;
	TArray<UPackage*> Packages;
	GatherLandscapePackages(Info, Packages);
	Result.ModifiedPackagePaths = GetPackagePaths(Packages);

	FLandscapeExtensionRegion FullRegion;
	Result.AfterHeight = GetHeightStatistics(Landscape, FullRegion);
	if (Request.bSaveAfterCreate)
	{
		FString Error;
		if (!SavePackages(Packages, Result.SaveResult, Error))
		{
			World->DestroyActor(Landscape);
			RaiseError(TEXT("LANDSCAPE_EXTENSION_CREATE_SAVE_FAILED"), Error);
			return {};
		}
	}
	return Result;
}

FLandscapeExtensionMutationResult ULandscapeExtensionToolset::ImportHeightmap(
	ALandscapeProxy* Landscape,
	const FLandscapeExtensionHeightmapRequest& Request)
{
	return LandscapeExtensionToolsetPrivate::ApplyHeightmap(Landscape, Request, false);
}

FLandscapeExtensionMutationResult ULandscapeExtensionToolset::ReimportHeightmap(
	ALandscapeProxy* Landscape,
	const FLandscapeExtensionHeightmapRequest& Request)
{
	return LandscapeExtensionToolsetPrivate::ApplyHeightmap(Landscape, Request, true);
}

FLandscapeExtensionMutationResult ULandscapeExtensionToolset::ImportWeightmap(
	ALandscapeProxy* InputLandscape,
	ULandscapeLayerInfoObject* LayerInfo,
	const FLandscapeExtensionWeightmapRequest& Request)
{
	using namespace LandscapeExtensionToolsetPrivate;
	FLandscapeExtensionMutationResult Result;
	Result.Operation = TEXT("ImportWeightmap");

	ULandscapeInfo* Info = nullptr;
	ALandscape* Landscape = nullptr;
	FString Error;
	if (!ResolveLandscape(InputLandscape, Info, Landscape, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_TARGET_INVALID"), Error);
		return {};
	}
	if (!IsValid(LayerInfo) || Info->GetLayerInfoIndex(LayerInfo) == INDEX_NONE)
	{
		RaiseError(
			TEXT("LANDSCAPE_EXTENSION_WEIGHT_LAYER_INVALID"),
			TEXT("LayerInfo must be a target layer assigned to the Landscape."));
		return {};
	}

	FLandscapeExtensionRegion FullRegion;
	FIntRect Extent;
	if (!ResolveExtent(Info, FullRegion, Extent, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_WORLD_PARTITION_PARTIAL"), Error);
		return {};
	}
	FGuid EditLayerGuid;
	if (!ParseEditLayerGuid(Landscape, Request.DestinationEditLayerGuid, true, EditLayerGuid, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_DESTINATION_LAYER_INVALID"), Error);
		return {};
	}

	TArray<uint8> ImportedData;
	FString ResolvedSource;
	if (!DecodeWeightmap(
		Request.SourceFile,
		Request.bFlipYAxis,
		LayerInfo->GetLayerName(),
		Extent,
		ImportedData,
		Result.Warnings,
		ResolvedSource,
		Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_SOURCE_INVALID"), Error);
		return {};
	}

	TArray<UPackage*> Packages;
	if (!SaveRecoveryPoint(Info, Packages, Result.RecoveryPackagePaths, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_RECOVERY_SAVE_FAILED"), Error);
		return {};
	}

	TArray<uint8> OriginalData;
	ReadWeightData(Info, EditLayerGuid, LayerInfo, Extent, OriginalData);
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Landscape Extension Import Weightmap")));
		ModifyLandscapeObjects(Info);
		WriteWeightData(Info, EditLayerGuid, LayerInfo, Extent, ImportedData);
	}

	TArray<uint8> VerifiedData;
	ReadWeightData(Info, EditLayerGuid, LayerInfo, Extent, VerifiedData);
	if (!VerifyExact(ImportedData, VerifiedData))
	{
		WriteWeightData(Info, EditLayerGuid, LayerInfo, Extent, OriginalData);
		TArray<uint8> RollbackData;
		ReadWeightData(Info, EditLayerGuid, LayerInfo, Extent, RollbackData);
		RaiseError(
			VerifyExact(OriginalData, RollbackData)
				? TEXT("LANDSCAPE_EXTENSION_WEIGHT_VERIFY_FAILED_ROLLED_BACK")
				: TEXT("LANDSCAPE_EXTENSION_WEIGHT_VERIFY_FAILED_RELOAD_REQUIRED"),
			TEXT("Weightmap post-verification failed."));
		return {};
	}

	Result.Landscape = Landscape;
	Result.SourceFile = ResolvedSource;
	Result.ModifiedPackagePaths = GetPackagePaths(Packages);
	if (Request.bSaveAfterSuccess && !SavePackages(Packages, Result.SaveResult, Error))
	{
		WriteWeightData(Info, EditLayerGuid, LayerInfo, Extent, OriginalData);
		RaiseError(TEXT("LANDSCAPE_EXTENSION_WEIGHT_SAVE_FAILED_ROLLED_BACK"), Error);
		return {};
	}
	return Result;
}

FLandscapeExtensionMutationResult ULandscapeExtensionToolset::SetLandscapeMaterial(
	ALandscapeProxy* InputLandscape,
	UMaterialInterface* Material)
{
	using namespace LandscapeExtensionToolsetPrivate;
	FLandscapeExtensionMutationResult Result;
	Result.Operation = TEXT("SetLandscapeMaterial");
	ULandscapeInfo* Info = nullptr;
	ALandscape* Landscape = nullptr;
	FString Error;
	if (!ResolveLandscape(InputLandscape, Info, Landscape, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_TARGET_INVALID"), Error);
		return {};
	}
	if (!IsValid(Material))
	{
		RaiseError(
			TEXT("LANDSCAPE_EXTENSION_MATERIAL_INVALID"),
			TEXT("Material must reference a loaded material interface."));
		return {};
	}

	TArray<UPackage*> Packages;
	if (!SaveRecoveryPoint(Info, Packages, Result.RecoveryPackagePaths, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_RECOVERY_SAVE_FAILED"), Error);
		return {};
	}

	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Landscape Extension Set Material")));
		ModifyLandscapeObjects(Info);
		Landscape->LandscapeMaterial = Material;
		Info->UpdateAllComponentMaterialInstances(true);
		Landscape->MarkPackageDirty();
	}
	if (Landscape->GetLandscapeMaterial() != Material)
	{
		RaiseError(
			TEXT("LANDSCAPE_EXTENSION_MATERIAL_VERIFY_FAILED"),
			TEXT("The canonical Landscape did not retain the requested material."));
		return {};
	}

	Result.Landscape = Landscape;
	Result.ModifiedPackagePaths = GetPackagePaths(Packages);
	if (!SavePackages(Packages, Result.SaveResult, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_MATERIAL_SAVE_FAILED"), Error);
		return {};
	}
	return Result;
}

FLandscapeExtensionHeightStatistics ULandscapeExtensionToolset::GetHeightStatistics(
	ALandscapeProxy* InputLandscape,
	const FLandscapeExtensionRegion& Region)
{
	using namespace LandscapeExtensionToolsetPrivate;
	ULandscapeInfo* Info = nullptr;
	ALandscape* Landscape = nullptr;
	FString Error;
	if (!ResolveLandscape(InputLandscape, Info, Landscape, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_TARGET_INVALID"), Error);
		return {};
	}
	FIntRect Extent;
	if (!ResolveExtent(Info, Region, Extent, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_REGION_INVALID"), Error);
		return {};
	}
	FGuid EditLayerGuid;
	if (!ParseEditLayerGuid(Landscape, Region.EditLayerGuid, false, EditLayerGuid, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_EDIT_LAYER_INVALID"), Error);
		return {};
	}

	TArray<uint16> Data;
	ReadHeightData(Info, EditLayerGuid, Extent, Data);
	return BuildStatistics(Landscape, Extent, Data);
}

FLandscapeExtensionBuildingPadValidation ULandscapeExtensionToolset::ValidateBuildingPad(
	ALandscapeProxy* InputLandscape,
	const FLandscapeExtensionBuildingPadRequest& Request)
{
	using namespace LandscapeExtensionToolsetPrivate;
	FLandscapeExtensionBuildingPadValidation Result;
	ULandscapeInfo* Info = nullptr;
	ALandscape* Landscape = nullptr;
	FString Error;
	if (!ResolveLandscape(InputLandscape, Info, Landscape, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_TARGET_INVALID"), Error);
		return {};
	}
	if (Request.HalfExtentsCentimeters.X <= 0.0
		|| Request.HalfExtentsCentimeters.Y <= 0.0
		|| Request.MaxHeightVariationCentimeters < 0.0
		|| Request.MaxSlopeDegrees < 0.0
		|| Request.MaxSlopeDegrees > 90.0)
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_PAD_REQUEST_INVALID"), TEXT("Pad extents and thresholds are invalid."));
		return {};
	}

	FGuid EditLayerGuid;
	if (!ParseEditLayerGuid(Landscape, Request.EditLayerGuid, false, EditLayerGuid, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_EDIT_LAYER_INVALID"), Error);
		return {};
	}

	FIntRect FullExtent;
	if (!Info->GetLandscapeExtent(FullExtent))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_EXTENT_UNAVAILABLE"), TEXT("The Landscape has no loaded extent."));
		return {};
	}

	const double X = Request.HalfExtentsCentimeters.X;
	const double Y = Request.HalfExtentsCentimeters.Y;
	const TArray<FVector2D> Offsets = {
		FVector2D(-X, -Y), FVector2D(X, -Y), FVector2D(X, Y), FVector2D(-X, Y),
		FVector2D(0.0, -Y), FVector2D(X, 0.0), FVector2D(0.0, Y), FVector2D(-X, 0.0),
		FVector2D::ZeroVector
	};
	const double SinYaw = FMath::Sin(FMath::DegreesToRadians(Request.YawDegrees));
	const double CosYaw = FMath::Cos(FMath::DegreesToRadians(Request.YawDegrees));
	const FTransform WorldToLandscape = Landscape->LandscapeActorToWorld().Inverse();
	FLandscapeEditDataInterface Edit(Info, EditLayerGuid, false);

	TArray<FVector> WorldSamples;
	for (const FVector2D& Offset : Offsets)
	{
		const FVector WorldPoint(
			Request.Center.X + Offset.X * CosYaw - Offset.Y * SinYaw,
			Request.Center.Y + Offset.X * SinYaw + Offset.Y * CosYaw,
			Request.Center.Z);
		const FVector Local = WorldToLandscape.TransformPosition(WorldPoint);
		const FIntPoint Vertex(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y));
		if (!FullExtent.Contains(Vertex)
			|| Info->HasUnloadedComponentsInRegion(Vertex.X, Vertex.Y, Vertex.X, Vertex.Y))
		{
			RaiseError(
				TEXT("LANDSCAPE_EXTENSION_PAD_SAMPLE_MISSING"),
				FString::Printf(TEXT("Pad sample at world location %s does not resolve to a loaded Landscape vertex."), *WorldPoint.ToString()));
			return {};
		}

		uint16 Raw = 0;
		Edit.GetHeightDataFast(Vertex.X, Vertex.Y, Vertex.X, Vertex.Y, &Raw, 1);
		FLandscapeExtensionHeightSample Sample;
		Sample.Vertex = Vertex;
		Sample.RawHeight = Raw;
		Sample.WorldZCentimeters = RawHeightToWorldZ(Landscape, Vertex, Raw);
		Result.Samples.Add(Sample);
		WorldSamples.Add(FVector(WorldPoint.X, WorldPoint.Y, Sample.WorldZCentimeters));
	}

	double MinZ = TNumericLimits<double>::Max();
	double MaxZ = TNumericLimits<double>::Lowest();
	FVector Mean = FVector::ZeroVector;
	for (const FVector& Point : WorldSamples)
	{
		Mean += Point;
		MinZ = FMath::Min(MinZ, Point.Z);
		MaxZ = FMath::Max(MaxZ, Point.Z);
	}
	Mean /= WorldSamples.Num();
	Result.HeightVariationCentimeters = MaxZ - MinZ;

	double Sxx = 0.0;
	double Sxy = 0.0;
	double Syy = 0.0;
	double Sxz = 0.0;
	double Syz = 0.0;
	for (const FVector& Point : WorldSamples)
	{
		const double Dx = Point.X - Mean.X;
		const double Dy = Point.Y - Mean.Y;
		const double Dz = Point.Z - Mean.Z;
		Sxx += Dx * Dx;
		Sxy += Dx * Dy;
		Syy += Dy * Dy;
		Sxz += Dx * Dz;
		Syz += Dy * Dz;
	}
	const double Determinant = Sxx * Syy - Sxy * Sxy;
	if (FMath::Abs(Determinant) > UE_DOUBLE_SMALL_NUMBER)
	{
		const double A = (Sxz * Syy - Syz * Sxy) / Determinant;
		const double B = (Syz * Sxx - Sxz * Sxy) / Determinant;
		Result.BestFitSlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(A * A + B * B)));
	}

	Result.bWithinHeightVariation =
		Result.HeightVariationCentimeters <= Request.MaxHeightVariationCentimeters;
	Result.bWithinSlope = Result.BestFitSlopeDegrees <= Request.MaxSlopeDegrees;
	return Result;
}

FLandscapeExtensionSaveResult ULandscapeExtensionToolset::SaveLandscape(ALandscapeProxy* Landscape)
{
	using namespace LandscapeExtensionToolsetPrivate;
	FLandscapeExtensionSaveResult Result;
	ULandscapeInfo* Info = nullptr;
	ALandscape* MainLandscape = nullptr;
	FString Error;
	if (!ResolveLandscape(Landscape, Info, MainLandscape, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_TARGET_INVALID"), Error);
		return {};
	}

	TArray<UPackage*> Packages;
	GatherLandscapePackages(Info, Packages);
	if (!SavePackages(Packages, Result, Error))
	{
		RaiseError(TEXT("LANDSCAPE_EXTENSION_SAVE_FAILED"), Error);
		return {};
	}
	return Result;
}
