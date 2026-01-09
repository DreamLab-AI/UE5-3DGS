// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CameraTrajectory.generated.h"

/**
 * Camera trajectory types for 3DGS capture
 */
UENUM(BlueprintType)
enum class ECameraTrajectoryType : uint8
{
	/** Spherical trajectory around a center point */
	Spherical UMETA(DisplayName = "Spherical"),

	/** Orbital rings at different elevation levels */
	Orbital UMETA(DisplayName = "Orbital Rings"),

	/** Spiral path from top to bottom */
	Spiral UMETA(DisplayName = "Spiral"),

	/** Grid-based viewpoints */
	Grid UMETA(DisplayName = "Grid"),

	/** Custom waypoint-based trajectory */
	Custom UMETA(DisplayName = "Custom Waypoints"),

	/** Hemisphere coverage (upper half of sphere) */
	Hemisphere UMETA(DisplayName = "Hemisphere"),

	/** 360° panoramic capture points */
	Panoramic360 UMETA(DisplayName = "360 Panoramic")
};

/**
 * Single camera viewpoint with position and rotation
 */
USTRUCT(BlueprintType)
struct UNREALTOGAUSSIAN_API FCameraViewpoint
{
	GENERATED_BODY()

	/** World position of the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	FVector Position = FVector::ZeroVector;

	/** World rotation of the camera (looking direction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Unique ID for this viewpoint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	int32 ViewpointId = 0;

	/** Ring/level index for orbital trajectories */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	int32 RingIndex = 0;

	/** Position within the ring (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	float RingPosition = 0.0f;

	/** Distance from focus point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	float Distance = 0.0f;

	/** Elevation angle in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	float ElevationAngle = 0.0f;

	/** Azimuth angle in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewpoint")
	float AzimuthAngle = 0.0f;

	FCameraViewpoint() = default;

	FCameraViewpoint(const FVector& InPosition, const FRotator& InRotation, int32 InId)
		: Position(InPosition), Rotation(InRotation), ViewpointId(InId) {}

	/** Get the camera transform */
	FTransform GetTransform() const
	{
		return FTransform(Rotation, Position);
	}
};

/**
 * Configuration for camera trajectory generation
 */
USTRUCT(BlueprintType)
struct UNREALTOGAUSSIAN_API FTrajectoryConfig
{
	GENERATED_BODY()

	/** Type of trajectory to generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	ECameraTrajectoryType TrajectoryType = ECameraTrajectoryType::Orbital;

	/** Center point to orbit around / look at */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FVector FocusPoint = FVector::ZeroVector;

	/** Base distance from focus point (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "100.0"))
	float BaseRadius = 500.0f;

	/** Number of orbital rings (for Orbital type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "1", ClampMax = "10"))
	int32 NumRings = 5;

	/** Number of viewpoints per ring */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "8", ClampMax = "72"))
	int32 ViewsPerRing = 36;

	/** Minimum elevation angle (degrees from horizon) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MinElevation = -30.0f;

	/** Maximum elevation angle (degrees from horizon) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MaxElevation = 60.0f;

	/** Starting azimuth angle (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	float StartAzimuth = 0.0f;

	/** Whether to vary radius per ring for better coverage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	bool bVaryRadiusPerRing = true;

	/** Radius variation factor (0.1 = 10% variation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RadiusVariation = 0.15f;

	/** Whether to offset azimuth between rings for better overlap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	bool bStaggerRings = true;

	/** Whether cameras should look at focus point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	bool bLookAtFocusPoint = true;

	/** Additional pitch offset (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	float PitchOffset = 0.0f;

	/** Custom waypoints (for Custom type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	TArray<FTransform> CustomWaypoints;

	/** Get total expected viewpoint count */
	int32 GetExpectedViewpointCount() const;
};

namespace UE5_3DGS
{
	/**
	 * Camera trajectory generator for 3DGS capture
	 *
	 * Research-backed defaults:
	 * - 3-5 orbital rings at different elevations
	 * - 24-36 views per ring for 60-80% image overlap
	 * - Total 100-180 views for typical scenes
	 * - Staggered ring positions for optimal coverage
	 */
	class UNREALTOGAUSSIAN_API FCameraTrajectoryGenerator
	{
	public:
		/**
		 * Generate camera viewpoints based on configuration
		 *
		 * @param Config Trajectory configuration
		 * @return Array of camera viewpoints
		 */
		static TArray<FCameraViewpoint> GenerateViewpoints(const FTrajectoryConfig& Config);

		/**
		 * Generate orbital ring viewpoints
		 * Recommended for most 3DGS captures
		 *
		 * @param Config Trajectory configuration
		 * @return Array of viewpoints arranged in orbital rings
		 */
		static TArray<FCameraViewpoint> GenerateOrbitalRings(const FTrajectoryConfig& Config);

		/**
		 * Generate spherical distribution viewpoints
		 * Uses Fibonacci sphere for even distribution
		 *
		 * @param Config Trajectory configuration
		 * @return Array of viewpoints with spherical distribution
		 */
		static TArray<FCameraViewpoint> GenerateSpherical(const FTrajectoryConfig& Config);

		/**
		 * Generate spiral trajectory viewpoints
		 *
		 * @param Config Trajectory configuration
		 * @return Array of viewpoints along a spiral path
		 */
		static TArray<FCameraViewpoint> GenerateSpiral(const FTrajectoryConfig& Config);

		/**
		 * Generate hemisphere viewpoints (upper half only)
		 * Good for architectural/room captures
		 *
		 * @param Config Trajectory configuration
		 * @return Array of viewpoints covering hemisphere
		 */
		static TArray<FCameraViewpoint> GenerateHemisphere(const FTrajectoryConfig& Config);

		/**
		 * Generate 360° panoramic capture points
		 * For VR/360 content generation
		 *
		 * @param Config Trajectory configuration
		 * @return Array of viewpoints for 360 capture
		 */
		static TArray<FCameraViewpoint> GeneratePanoramic360(const FTrajectoryConfig& Config);

		/**
		 * Calculate optimal configuration for a bounding box
		 *
		 * @param BoundingBox Scene bounding box
		 * @param DesiredOverlap Target image overlap (0.6-0.8 recommended)
		 * @param HorizontalFOV Camera horizontal FOV in degrees
		 * @return Recommended trajectory configuration
		 */
		static FTrajectoryConfig CalculateOptimalConfig(
			const FBox& BoundingBox,
			float DesiredOverlap = 0.7f,
			float HorizontalFOV = 90.0f
		);

		/**
		 * Validate trajectory configuration
		 *
		 * @param Config Configuration to validate
		 * @param OutWarnings Output warnings
		 * @return True if valid
		 */
		static bool ValidateConfig(
			const FTrajectoryConfig& Config,
			TArray<FString>& OutWarnings
		);

		/**
		 * Calculate estimated overlap between adjacent viewpoints
		 *
		 * @param Viewpoints Array of viewpoints
		 * @param HorizontalFOV Camera FOV in degrees
		 * @return Average overlap percentage (0-1)
		 */
		static float CalculateAverageOverlap(
			const TArray<FCameraViewpoint>& Viewpoints,
			float HorizontalFOV
		);

	private:
		/** Calculate rotation to look at target from position */
		static FRotator CalculateLookAtRotation(
			const FVector& CameraPosition,
			const FVector& TargetPosition,
			float PitchOffset = 0.0f
		);

		/** Get position on sphere given angles and radius */
		static FVector SphericalToCartesian(
			float Radius,
			float ElevationDegrees,
			float AzimuthDegrees,
			const FVector& Center
		);

		/** Fibonacci sphere point distribution for even coverage */
		static FVector FibonacciSpherePoint(
			int32 Index,
			int32 TotalPoints,
			float Radius,
			const FVector& Center
		);
	};
}
