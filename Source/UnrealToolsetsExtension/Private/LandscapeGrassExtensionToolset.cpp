#include "LandscapeGrassExtensionToolset.h"

#include "Kismet/KismetSystemLibrary.h"
#include "LandscapeGrassType.h"
#include "ScopedTransaction.h"

namespace LandscapeGrassExtensionToolsetPrivate
{
	void RaiseError(const TCHAR* Code, const FString& Detail)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("[%s] %s"), Code, *Detail));
	}

	bool ValidateGrassType(ULandscapeGrassType* GrassType, FString& OutError)
	{
		if (!IsValid(GrassType) || GrassType->HasAnyFlags(RF_Transient))
		{
			OutError = TEXT("GrassType must reference a loaded, non-transient LandscapeGrassType asset.");
			return false;
		}
		return true;
	}

	bool ValidateScaleRange(
		const TCHAR* Name,
		const FVector2D& Range,
		FString& OutError)
	{
		if (!FMath::IsFinite(Range.X)
			|| !FMath::IsFinite(Range.Y)
			|| Range.X <= 0.0
			|| Range.Y <= 0.0
			|| Range.X > Range.Y)
		{
			OutError = FString::Printf(
				TEXT("%s must contain finite positive values ordered minimum to maximum."),
				Name);
			return false;
		}
		return true;
	}

	bool ValidateVariety(
		const FLandscapeGrassVarietyRequest& Variety,
		FString& OutError)
	{
		if (Variety.Mesh.IsNull())
		{
			OutError = TEXT("Mesh must reference a static mesh asset.");
			return false;
		}
		if (!FMath::IsFinite(Variety.GrassDensity)
			|| Variety.GrassDensity < 0.0f
			|| Variety.GrassDensity > 1000.0f)
		{
			OutError = TEXT("GrassDensity must be a finite value in the range 0..1000.");
			return false;
		}
		if (!FMath::IsFinite(Variety.PlacementJitter)
			|| Variety.PlacementJitter < 0.0f
			|| Variety.PlacementJitter > 1.0f)
		{
			OutError = TEXT("PlacementJitter must be a finite value in the range 0..1.");
			return false;
		}
		if (Variety.StartCullDistanceCentimeters < 0
			|| Variety.EndCullDistanceCentimeters < 0
			|| (Variety.EndCullDistanceCentimeters > 0
				&& Variety.EndCullDistanceCentimeters
					< Variety.StartCullDistanceCentimeters))
		{
			OutError = TEXT("Cull distances must be non-negative, and a non-zero end distance must not precede the start distance.");
			return false;
		}
		if (Variety.MinLOD < -1 || Variety.MinLOD > 8)
		{
			OutError = TEXT("MinLOD must be in the range -1..8.");
			return false;
		}
		return ValidateScaleRange(TEXT("ScaleX"), Variety.ScaleX, OutError)
			&& ValidateScaleRange(TEXT("ScaleY"), Variety.ScaleY, OutError)
			&& ValidateScaleRange(TEXT("ScaleZ"), Variety.ScaleZ, OutError);
	}

	EGrassScaling BuildNativeScaling(ELandscapeGrassExtensionScaling Scaling)
	{
		switch (Scaling)
		{
		case ELandscapeGrassExtensionScaling::Free:
			return EGrassScaling::Free;
		case ELandscapeGrassExtensionScaling::LockXY:
			return EGrassScaling::LockXY;
		default:
			return EGrassScaling::Uniform;
		}
	}

	ELandscapeGrassExtensionScaling BuildPublicScaling(EGrassScaling Scaling)
	{
		switch (Scaling)
		{
		case EGrassScaling::Free:
			return ELandscapeGrassExtensionScaling::Free;
		case EGrassScaling::LockXY:
			return ELandscapeGrassExtensionScaling::LockXY;
		default:
			return ELandscapeGrassExtensionScaling::Uniform;
		}
	}

	FGrassVariety BuildNativeVariety(
		const FLandscapeGrassVarietyRequest& Variety,
		FString& OutError)
	{
		FGrassVariety NativeVariety;
		NativeVariety.GrassMesh = Variety.Mesh.LoadSynchronous();
		if (!IsValid(NativeVariety.GrassMesh))
		{
			OutError = FString::Printf(
				TEXT("Mesh could not be loaded: %s"),
				*Variety.Mesh.ToString());
			return {};
		}

		NativeVariety.GrassDensity = FPerPlatformFloat(Variety.GrassDensity);
		NativeVariety.GrassDensityQuality.Default = Variety.GrassDensity;
		NativeVariety.bUseGrid = Variety.bUseGrid;
		NativeVariety.PlacementJitter = Variety.PlacementJitter;
		NativeVariety.StartCullDistance =
			FPerPlatformInt(Variety.StartCullDistanceCentimeters);
		NativeVariety.StartCullDistanceQuality.Default =
			Variety.StartCullDistanceCentimeters;
		NativeVariety.EndCullDistance =
			FPerPlatformInt(Variety.EndCullDistanceCentimeters);
		NativeVariety.EndCullDistanceQuality.Default =
			Variety.EndCullDistanceCentimeters;
		NativeVariety.MinLOD = Variety.MinLOD;
		NativeVariety.Scaling = BuildNativeScaling(Variety.Scaling);
		NativeVariety.ScaleX =
			FFloatInterval(Variety.ScaleX.X, Variety.ScaleX.Y);
		NativeVariety.ScaleY =
			FFloatInterval(Variety.ScaleY.X, Variety.ScaleY.Y);
		NativeVariety.ScaleZ =
			FFloatInterval(Variety.ScaleZ.X, Variety.ScaleZ.Y);
		NativeVariety.RandomRotation = Variety.bRandomRotation;
		NativeVariety.AlignToSurface = Variety.bAlignToSurface;
		NativeVariety.bReceivesDecals = Variety.bReceivesDecals;
		NativeVariety.bCastDynamicShadow = Variety.bCastDynamicShadow;
		return NativeVariety;
	}

	FLandscapeGrassVariety BuildPublicVariety(
		const FGrassVariety& NativeVariety,
		int32 Index)
	{
		FLandscapeGrassVariety Variety;
		Variety.Index = Index;
		Variety.Mesh = NativeVariety.GrassMesh;
		Variety.GrassDensity = NativeVariety.GrassDensity.Default;
		Variety.bUseGrid = NativeVariety.bUseGrid;
		Variety.PlacementJitter = NativeVariety.PlacementJitter;
		Variety.StartCullDistanceCentimeters =
			NativeVariety.StartCullDistance.Default;
		Variety.EndCullDistanceCentimeters =
			NativeVariety.EndCullDistance.Default;
		Variety.MinLOD = NativeVariety.MinLOD;
		Variety.Scaling = BuildPublicScaling(NativeVariety.Scaling);
		Variety.ScaleX = FVector2D(
			NativeVariety.ScaleX.Min,
			NativeVariety.ScaleX.Max);
		Variety.ScaleY = FVector2D(
			NativeVariety.ScaleY.Min,
			NativeVariety.ScaleY.Max);
		Variety.ScaleZ = FVector2D(
			NativeVariety.ScaleZ.Min,
			NativeVariety.ScaleZ.Max);
		Variety.bRandomRotation = NativeVariety.RandomRotation;
		Variety.bAlignToSurface = NativeVariety.AlignToSurface;
		Variety.bReceivesDecals = NativeVariety.bReceivesDecals;
		Variety.bCastDynamicShadow = NativeVariety.bCastDynamicShadow;
		return Variety;
	}

	TArray<FLandscapeGrassVariety> BuildPublicVarieties(
		const ULandscapeGrassType* GrassType)
	{
		TArray<FLandscapeGrassVariety> Varieties;
		if (!IsValid(GrassType))
		{
			return Varieties;
		}
		Varieties.Reserve(GrassType->GrassVarieties.Num());
		for (int32 Index = 0; Index < GrassType->GrassVarieties.Num(); ++Index)
		{
			Varieties.Add(
				BuildPublicVariety(GrassType->GrassVarieties[Index], Index));
		}
		return Varieties;
	}

	FLandscapeGrassMutation FinishMutation(ULandscapeGrassType* GrassType)
	{
		GrassType->PostEditChange();
		GrassType->MarkPackageDirty();

		FLandscapeGrassMutation Result;
		Result.GrassType = GrassType;
		Result.ModifiedPackagePath = GrassType->GetOutermost()->GetName();
		Result.bEnableDensityScaling = GrassType->bEnableDensityScaling;
		Result.Varieties = BuildPublicVarieties(GrassType);
		return Result;
	}

	FLandscapeGrassMutation MutateVarieties(
		ULandscapeGrassType* GrassType,
		TFunctionRef<bool(TArray<FGrassVariety>&, FString&)> Mutation)
	{
		FString Error;
		if (!ValidateGrassType(GrassType, Error))
		{
			RaiseError(TEXT("LANDSCAPE_GRASS_TYPE_INVALID"), Error);
			return {};
		}

		TArray<FGrassVariety> NewVarieties = GrassType->GrassVarieties;
		if (!Mutation(NewVarieties, Error))
		{
			RaiseError(TEXT("LANDSCAPE_GRASS_VARIETY_INVALID"), Error);
			return {};
		}

		const FScopedTransaction Transaction(
			FText::FromString(TEXT("Unreal Toolsets Extension Edit Landscape Grass Varieties")));
		GrassType->Modify();
		GrassType->GrassVarieties = MoveTemp(NewVarieties);
		return FinishMutation(GrassType);
	}
}

TArray<FLandscapeGrassVariety>
ULandscapeGrassExtensionToolset::GetGrassVarieties(
	ULandscapeGrassType* GrassType)
{
	using namespace LandscapeGrassExtensionToolsetPrivate;
	FString Error;
	if (!ValidateGrassType(GrassType, Error))
	{
		RaiseError(TEXT("LANDSCAPE_GRASS_TYPE_INVALID"), Error);
		return {};
	}
	return BuildPublicVarieties(GrassType);
}

FLandscapeGrassMutation
ULandscapeGrassExtensionToolset::ReplaceGrassVarieties(
	ULandscapeGrassType* GrassType,
	const TArray<FLandscapeGrassVarietyRequest>& Varieties)
{
	using namespace LandscapeGrassExtensionToolsetPrivate;
	return MutateVarieties(
		GrassType,
		[&Varieties](TArray<FGrassVariety>& NativeVarieties, FString& Error)
		{
			NativeVarieties.Reset();
			NativeVarieties.Reserve(Varieties.Num());
			for (const FLandscapeGrassVarietyRequest& Variety : Varieties)
			{
				if (!ValidateVariety(Variety, Error))
				{
					return false;
				}
				FGrassVariety NativeVariety =
					BuildNativeVariety(Variety, Error);
				if (!Error.IsEmpty())
				{
					return false;
				}
				NativeVarieties.Add(MoveTemp(NativeVariety));
			}
			return true;
		});
}

FLandscapeGrassMutation ULandscapeGrassExtensionToolset::AddGrassVariety(
	ULandscapeGrassType* GrassType,
	const FLandscapeGrassVarietyRequest& Variety,
	int32 InsertIndex)
{
	using namespace LandscapeGrassExtensionToolsetPrivate;
	return MutateVarieties(
		GrassType,
		[&Variety, InsertIndex](TArray<FGrassVariety>& Varieties, FString& Error)
		{
			if (!ValidateVariety(Variety, Error))
			{
				return false;
			}
			const int32 ResolvedIndex =
				InsertIndex == INDEX_NONE ? Varieties.Num() : InsertIndex;
			if (ResolvedIndex < 0 || ResolvedIndex > Varieties.Num())
			{
				Error = FString::Printf(
					TEXT("InsertIndex %d is outside the valid range 0..%d."),
					ResolvedIndex,
					Varieties.Num());
				return false;
			}
			FGrassVariety NativeVariety = BuildNativeVariety(Variety, Error);
			if (!Error.IsEmpty())
			{
				return false;
			}
			Varieties.Insert(MoveTemp(NativeVariety), ResolvedIndex);
			return true;
		});
}

FLandscapeGrassMutation ULandscapeGrassExtensionToolset::UpdateGrassVariety(
	ULandscapeGrassType* GrassType,
	int32 Index,
	const FLandscapeGrassVarietyRequest& Variety)
{
	using namespace LandscapeGrassExtensionToolsetPrivate;
	return MutateVarieties(
		GrassType,
		[Index, &Variety](TArray<FGrassVariety>& Varieties, FString& Error)
		{
			if (!Varieties.IsValidIndex(Index))
			{
				Error = FString::Printf(
					TEXT("Index %d is outside the grass variety array."),
					Index);
				return false;
			}
			if (!ValidateVariety(Variety, Error))
			{
				return false;
			}
			FGrassVariety NativeVariety = BuildNativeVariety(Variety, Error);
			if (!Error.IsEmpty())
			{
				return false;
			}
			Varieties[Index] = MoveTemp(NativeVariety);
			return true;
		});
}

FLandscapeGrassMutation ULandscapeGrassExtensionToolset::RemoveGrassVariety(
	ULandscapeGrassType* GrassType,
	int32 Index)
{
	using namespace LandscapeGrassExtensionToolsetPrivate;
	return MutateVarieties(
		GrassType,
		[Index](TArray<FGrassVariety>& Varieties, FString& Error)
		{
			if (!Varieties.IsValidIndex(Index))
			{
				Error = FString::Printf(
					TEXT("Index %d is outside the grass variety array."),
					Index);
				return false;
			}
			Varieties.RemoveAt(Index);
			return true;
		});
}

FLandscapeGrassMutation
ULandscapeGrassExtensionToolset::ReorderGrassVariety(
	ULandscapeGrassType* GrassType,
	int32 FromIndex,
	int32 ToIndex)
{
	using namespace LandscapeGrassExtensionToolsetPrivate;
	return MutateVarieties(
		GrassType,
		[FromIndex, ToIndex](TArray<FGrassVariety>& Varieties, FString& Error)
		{
			if (!Varieties.IsValidIndex(FromIndex)
				|| !Varieties.IsValidIndex(ToIndex))
			{
				Error = FString::Printf(
					TEXT("FromIndex %d and ToIndex %d must both reference existing varieties."),
					FromIndex,
					ToIndex);
				return false;
			}
			if (FromIndex != ToIndex)
			{
				FGrassVariety MovedVariety = Varieties[FromIndex];
				Varieties.RemoveAt(FromIndex);
				Varieties.Insert(MoveTemp(MovedVariety), ToIndex);
			}
			return true;
		});
}

FLandscapeGrassMutation
ULandscapeGrassExtensionToolset::SetGrassDensityScaling(
	ULandscapeGrassType* GrassType,
	bool bEnableDensityScaling)
{
	using namespace LandscapeGrassExtensionToolsetPrivate;
	FString Error;
	if (!ValidateGrassType(GrassType, Error))
	{
		RaiseError(TEXT("LANDSCAPE_GRASS_TYPE_INVALID"), Error);
		return {};
	}

	const FScopedTransaction Transaction(
		FText::FromString(TEXT("Unreal Toolsets Extension Set Landscape Grass Density Scaling")));
	GrassType->Modify();
	GrassType->bEnableDensityScaling = bEnableDensityScaling;
	return FinishMutation(GrassType);
}
