#include "PlacementExtensionToolset.h"

#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LandscapeProxy.h"
#include "ScopedTransaction.h"

namespace PlacementExtensionToolsetPrivate
{
	void RaiseError(const TCHAR* Code, const FString& Detail)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("[%s] %s"), Code, *Detail));
	}
}

FPlacementExtensionSnapResult UPlacementExtensionToolset::SnapActorToGround(
	AActor* Actor,
	const FPlacementExtensionSnapRequest& Request)
{
	using namespace PlacementExtensionToolsetPrivate;

	if (!IsValid(Actor) || !IsValid(Actor->GetWorld()))
	{
		RaiseError(
			TEXT("PLACEMENT_EXTENSION_ACTOR_INVALID"),
			TEXT("Actor must reference a loaded Actor in a valid world."));
		return {};
	}
	if (Request.TraceUpCentimeters <= 0.0 || Request.TraceDownCentimeters <= 0.0)
	{
		RaiseError(
			TEXT("PLACEMENT_EXTENSION_TRACE_RANGE_INVALID"),
			TEXT("TraceUpCentimeters and TraceDownCentimeters must both be greater than zero."));
		return {};
	}

	FVector BoundsOrigin = Actor->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Actor->GetActorBounds(false, BoundsOrigin, BoundsExtent, true);
	const double BoundsBottom = Request.bUseActorBoundsBottom
		? BoundsOrigin.Z - BoundsExtent.Z
		: Actor->GetActorLocation().Z;

	const FVector TraceOrigin(Actor->GetActorLocation().X, Actor->GetActorLocation().Y, BoundsOrigin.Z);
	const FVector TraceStart = TraceOrigin + FVector(0.0, 0.0, Request.TraceUpCentimeters);
	const FVector TraceEnd = TraceOrigin - FVector(0.0, 0.0, Request.TraceDownCentimeters);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlacementExtensionSnapActorToGround), Request.bTraceComplex);
	QueryParams.AddIgnoredActor(Actor);
	for (AActor* IgnoredActor : Request.AdditionalIgnoredActors)
	{
		if (IsValid(IgnoredActor))
		{
			QueryParams.AddIgnoredActor(IgnoredActor);
		}
	}
	if (Request.bIgnoreAttachedActors)
	{
		TArray<AActor*> AttachedActors;
		Actor->GetAttachedActors(AttachedActors, true, true);
		QueryParams.AddIgnoredActors(AttachedActors);
	}

	TArray<FHitResult> Hits;
	Actor->GetWorld()->LineTraceMultiByChannel(
		Hits,
		TraceStart,
		TraceEnd,
		Request.TraceChannel,
		QueryParams);

	const FHitResult* SelectedHit = nullptr;
	const FHitResult* FirstAcceptedHit = nullptr;
	const FHitResult* FirstLandscapeHit = nullptr;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor) || HitActor == Actor || HitActor->IsAttachedTo(Actor))
		{
			continue;
		}

		const bool bLandscapeHit = HitActor->IsA<ALandscapeProxy>();
		if (Request.bLandscapeOnly && !bLandscapeHit)
		{
			continue;
		}
		if (FirstAcceptedHit == nullptr)
		{
			FirstAcceptedHit = &Hit;
		}
		if (bLandscapeHit && FirstLandscapeHit == nullptr)
		{
			FirstLandscapeHit = &Hit;
		}
	}
	SelectedHit = Request.bPreferLandscape && FirstLandscapeHit != nullptr
		? FirstLandscapeHit
		: FirstAcceptedHit;

	if (SelectedHit == nullptr)
	{
		RaiseError(
			TEXT("PLACEMENT_EXTENSION_NO_GROUND_HIT"),
			FString::Printf(
				TEXT("No accepted ground hit was found from Z %.2f to %.2f on trace channel %s."),
				TraceStart.Z,
				TraceEnd.Z,
				*UEnum::GetValueAsString(Request.TraceChannel.GetValue())));
		return {};
	}

	FPlacementExtensionSnapResult Result;
	Result.Actor = Actor;
	Result.HitActor = SelectedHit->GetActor();
	Result.HitComponentPath = IsValid(SelectedHit->GetComponent())
		? SelectedHit->GetComponent()->GetPathName()
		: FString();
	Result.PreviousLocation = Actor->GetActorLocation();
	Result.HitLocation = SelectedHit->ImpactPoint;
	Result.HitNormal = SelectedHit->ImpactNormal;
	Result.BoundsBottomBeforeCentimeters = BoundsBottom;
	Result.AppliedDeltaZCentimeters = SelectedHit->ImpactPoint.Z - BoundsBottom;
	Result.NewLocation = Result.PreviousLocation + FVector(0.0, 0.0, Result.AppliedDeltaZCentimeters);
	Result.TraceChannel = UEnum::GetValueAsString(Request.TraceChannel.GetValue());

	const FScopedTransaction Transaction(FText::FromString(TEXT("Unreal Toolsets Extension Snap Actor To Ground")));
	Actor->Modify();
	if (!Actor->SetActorLocation(Result.NewLocation, false, nullptr, ETeleportType::TeleportPhysics))
	{
		RaiseError(
			TEXT("PLACEMENT_EXTENSION_MOVE_FAILED"),
			FString::Printf(TEXT("Actor %s rejected the snapped location."), *Actor->GetPathName()));
		return {};
	}
	Actor->MarkPackageDirty();
	Result.NewLocation = Actor->GetActorLocation();
	return Result;
}
