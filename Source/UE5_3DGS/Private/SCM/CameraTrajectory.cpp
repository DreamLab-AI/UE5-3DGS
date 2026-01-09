// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "SCM/CameraTrajectory.h"

int32 FTrajectoryConfig::GetExpectedViewpointCount() const
{
	switch (TrajectoryType)
	{
	case ECameraTrajectoryType::Orbital:
		return NumRings * ViewsPerRing;

	case ECameraTrajectoryType::Spherical:
	case ECameraTrajectoryType::Hemisphere:
		return NumRings * ViewsPerRing; // Approximate

	case ECameraTrajectoryType::Spiral:
		return ViewsPerRing * 3; // Typically 3 full rotations

	case ECameraTrajectoryType::Grid:
		return FMath::Square(FMath::Sqrt(static_cast<float>(ViewsPerRing * NumRings)));

	case ECameraTrajectoryType::Panoramic360:
		return 6 * ViewsPerRing; // 6 faces * views per face

	case ECameraTrajectoryType::Custom:
		return CustomWaypoints.Num();

	default:
		return NumRings * ViewsPerRing;
	}
}

namespace UE5_3DGS
{
	TArray<FCameraViewpoint> FCameraTrajectoryGenerator::GenerateViewpoints(const FTrajectoryConfig& Config)
	{
		switch (Config.TrajectoryType)
		{
		case ECameraTrajectoryType::Orbital:
			return GenerateOrbitalRings(Config);

		case ECameraTrajectoryType::Spherical:
			return GenerateSpherical(Config);

		case ECameraTrajectoryType::Spiral:
			return GenerateSpiral(Config);

		case ECameraTrajectoryType::Hemisphere:
			return GenerateHemisphere(Config);

		case ECameraTrajectoryType::Panoramic360:
			return GeneratePanoramic360(Config);

		case ECameraTrajectoryType::Custom:
		{
			TArray<FCameraViewpoint> Result;
			for (int32 i = 0; i < Config.CustomWaypoints.Num(); ++i)
			{
				const FTransform& WP = Config.CustomWaypoints[i];
				FCameraViewpoint VP;
				VP.Position = WP.GetLocation();
				VP.Rotation = WP.GetRotation().Rotator();
				VP.ViewpointId = i;
				Result.Add(VP);
			}
			return Result;
		}

		default:
			return GenerateOrbitalRings(Config);
		}
	}

	TArray<FCameraViewpoint> FCameraTrajectoryGenerator::GenerateOrbitalRings(const FTrajectoryConfig& Config)
	{
		TArray<FCameraViewpoint> Viewpoints;
		int32 ViewpointId = 0;

		// Calculate elevation step
		float ElevationRange = Config.MaxElevation - Config.MinElevation;
		float ElevationStep = (Config.NumRings > 1) ? ElevationRange / (Config.NumRings - 1) : 0.0f;

		// Generate each ring
		for (int32 RingIdx = 0; RingIdx < Config.NumRings; ++RingIdx)
		{
			// Calculate elevation for this ring
			float Elevation = Config.MinElevation + (ElevationStep * RingIdx);

			// Calculate radius with optional variation
			float RingRadius = Config.BaseRadius;
			if (Config.bVaryRadiusPerRing)
			{
				// Vary radius sinusoidally across rings
				float Variation = FMath::Sin(RingIdx * PI / Config.NumRings);
				RingRadius *= (1.0f + Config.RadiusVariation * Variation);
			}

			// Calculate azimuth offset for staggered rings
			float AzimuthOffset = Config.StartAzimuth;
			if (Config.bStaggerRings)
			{
				// Offset alternate rings by half the angular step
				float AngularStep = 360.0f / Config.ViewsPerRing;
				AzimuthOffset += (RingIdx % 2) * (AngularStep / 2.0f);
			}

			// Generate viewpoints around this ring
			for (int32 ViewIdx = 0; ViewIdx < Config.ViewsPerRing; ++ViewIdx)
			{
				float Azimuth = AzimuthOffset + (ViewIdx * 360.0f / Config.ViewsPerRing);

				// Calculate position
				FVector Position = SphericalToCartesian(
					RingRadius, Elevation, Azimuth, Config.FocusPoint
				);

				// Calculate rotation
				FRotator Rotation;
				if (Config.bLookAtFocusPoint)
				{
					Rotation = CalculateLookAtRotation(Position, Config.FocusPoint, Config.PitchOffset);
				}
				else
				{
					// Point along the tangent of the orbital path
					float TangentAzimuth = Azimuth + 90.0f;
					Rotation = FRotator(-Elevation, TangentAzimuth, 0.0f);
				}

				// Create viewpoint
				FCameraViewpoint VP;
				VP.Position = Position;
				VP.Rotation = Rotation;
				VP.ViewpointId = ViewpointId++;
				VP.RingIndex = RingIdx;
				VP.RingPosition = static_cast<float>(ViewIdx) / Config.ViewsPerRing;
				VP.Distance = RingRadius;
				VP.ElevationAngle = Elevation;
				VP.AzimuthAngle = Azimuth;

				Viewpoints.Add(VP);
			}
		}

		return Viewpoints;
	}

	TArray<FCameraViewpoint> FCameraTrajectoryGenerator::GenerateSpherical(const FTrajectoryConfig& Config)
	{
		TArray<FCameraViewpoint> Viewpoints;

		int32 TotalPoints = Config.NumRings * Config.ViewsPerRing;

		for (int32 i = 0; i < TotalPoints; ++i)
		{
			// Use Fibonacci sphere for even distribution
			FVector Position = FibonacciSpherePoint(i, TotalPoints, Config.BaseRadius, Config.FocusPoint);

			// Filter based on elevation constraints
			FVector LocalPos = Position - Config.FocusPoint;
			float Elevation = FMath::RadiansToDegrees(FMath::Asin(LocalPos.Z / Config.BaseRadius));

			if (Elevation < Config.MinElevation || Elevation > Config.MaxElevation)
			{
				continue;
			}

			// Calculate rotation
			FRotator Rotation;
			if (Config.bLookAtFocusPoint)
			{
				Rotation = CalculateLookAtRotation(Position, Config.FocusPoint, Config.PitchOffset);
			}
			else
			{
				Rotation = LocalPos.Rotation();
			}

			FCameraViewpoint VP;
			VP.Position = Position;
			VP.Rotation = Rotation;
			VP.ViewpointId = Viewpoints.Num();
			VP.Distance = Config.BaseRadius;
			VP.ElevationAngle = Elevation;
			VP.AzimuthAngle = FMath::RadiansToDegrees(FMath::Atan2(LocalPos.Y, LocalPos.X));

			Viewpoints.Add(VP);
		}

		return Viewpoints;
	}

	TArray<FCameraViewpoint> FCameraTrajectoryGenerator::GenerateSpiral(const FTrajectoryConfig& Config)
	{
		TArray<FCameraViewpoint> Viewpoints;

		int32 TotalPoints = Config.ViewsPerRing * 3; // 3 full rotations
		float ElevationRange = Config.MaxElevation - Config.MinElevation;

		for (int32 i = 0; i < TotalPoints; ++i)
		{
			float T = static_cast<float>(i) / (TotalPoints - 1);

			// Elevation progresses linearly from max to min
			float Elevation = Config.MaxElevation - (T * ElevationRange);

			// Azimuth increases continuously (3 full rotations)
			float Azimuth = Config.StartAzimuth + (T * 360.0f * 3.0f);

			// Optional radius variation
			float Radius = Config.BaseRadius;
			if (Config.bVaryRadiusPerRing)
			{
				Radius *= (1.0f + Config.RadiusVariation * FMath::Sin(T * PI * 2.0f));
			}

			FVector Position = SphericalToCartesian(Radius, Elevation, Azimuth, Config.FocusPoint);

			FRotator Rotation;
			if (Config.bLookAtFocusPoint)
			{
				Rotation = CalculateLookAtRotation(Position, Config.FocusPoint, Config.PitchOffset);
			}

			FCameraViewpoint VP;
			VP.Position = Position;
			VP.Rotation = Rotation;
			VP.ViewpointId = i;
			VP.Distance = Radius;
			VP.ElevationAngle = Elevation;
			VP.AzimuthAngle = FMath::Fmod(Azimuth, 360.0f);

			Viewpoints.Add(VP);
		}

		return Viewpoints;
	}

	TArray<FCameraViewpoint> FCameraTrajectoryGenerator::GenerateHemisphere(const FTrajectoryConfig& Config)
	{
		// Override elevation range for hemisphere (0 to max)
		FTrajectoryConfig HemisphereConfig = Config;
		HemisphereConfig.MinElevation = FMath::Max(0.0f, Config.MinElevation);
		HemisphereConfig.MaxElevation = FMath::Min(85.0f, Config.MaxElevation);

		return GenerateOrbitalRings(HemisphereConfig);
	}

	TArray<FCameraViewpoint> FCameraTrajectoryGenerator::GeneratePanoramic360(const FTrajectoryConfig& Config)
	{
		TArray<FCameraViewpoint> Viewpoints;

		// 6 cardinal directions for cubemap-style capture
		TArray<FRotator> Directions = {
			FRotator(0, 0, 0),    // Forward
			FRotator(0, 90, 0),   // Right
			FRotator(0, 180, 0),  // Back
			FRotator(0, 270, 0),  // Left
			FRotator(-90, 0, 0),  // Up
			FRotator(90, 0, 0)    // Down
		};

		// Generate positions along a path through the scene
		int32 NumPositions = Config.ViewsPerRing;
		float PathLength = Config.BaseRadius * 2.0f;
		float Step = PathLength / (NumPositions - 1);

		int32 ViewpointId = 0;
		for (int32 PosIdx = 0; PosIdx < NumPositions; ++PosIdx)
		{
			// Position along a line through focus point
			FVector Position = Config.FocusPoint + FVector(
				-PathLength / 2.0f + Step * PosIdx,
				0.0f,
				0.0f
			);

			// Capture all 6 directions at each position
			for (int32 DirIdx = 0; DirIdx < Directions.Num(); ++DirIdx)
			{
				FCameraViewpoint VP;
				VP.Position = Position;
				VP.Rotation = Directions[DirIdx];
				VP.ViewpointId = ViewpointId++;
				VP.RingIndex = PosIdx;
				VP.RingPosition = static_cast<float>(DirIdx) / Directions.Num();

				Viewpoints.Add(VP);
			}
		}

		return Viewpoints;
	}

	FTrajectoryConfig FCameraTrajectoryGenerator::CalculateOptimalConfig(
		const FBox& BoundingBox,
		float DesiredOverlap,
		float HorizontalFOV)
	{
		FTrajectoryConfig Config;
		Config.TrajectoryType = ECameraTrajectoryType::Orbital;

		// Calculate focus point at box center
		Config.FocusPoint = BoundingBox.GetCenter();

		// Calculate radius based on bounding box size
		FVector BoxExtent = BoundingBox.GetExtent();
		float MaxExtent = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);

		// Distance to see entire object: d = extent / tan(FOV/2)
		float FOVRadians = FMath::DegreesToRadians(HorizontalFOV);
		float MinDistance = MaxExtent / FMath::Tan(FOVRadians / 2.0f);

		// Add margin (1.2x to 1.5x)
		Config.BaseRadius = MinDistance * 1.3f;

		// Calculate views per ring for desired overlap
		// Overlap = 1 - (angular_step / FOV)
		// angular_step = FOV * (1 - overlap)
		float AngularStep = HorizontalFOV * (1.0f - DesiredOverlap);
		Config.ViewsPerRing = FMath::CeilToInt(360.0f / AngularStep);
		Config.ViewsPerRing = FMath::Clamp(Config.ViewsPerRing, 12, 72);

		// Calculate number of rings based on vertical extent
		float VerticalFOV = HorizontalFOV / (16.0f / 9.0f); // Assume 16:9 aspect
		float VerticalAngularStep = VerticalFOV * (1.0f - DesiredOverlap);
		float ElevationRange = Config.MaxElevation - Config.MinElevation;
		Config.NumRings = FMath::CeilToInt(ElevationRange / VerticalAngularStep);
		Config.NumRings = FMath::Clamp(Config.NumRings, 3, 8);

		Config.bStaggerRings = true;
		Config.bVaryRadiusPerRing = true;
		Config.bLookAtFocusPoint = true;

		return Config;
	}

	bool FCameraTrajectoryGenerator::ValidateConfig(
		const FTrajectoryConfig& Config,
		TArray<FString>& OutWarnings)
	{
		OutWarnings.Empty();
		bool bIsValid = true;

		// Check viewpoint count
		int32 TotalViews = Config.GetExpectedViewpointCount();
		if (TotalViews < 50)
		{
			OutWarnings.Add(FString::Printf(
				TEXT("Low viewpoint count (%d). 100-180 recommended for quality 3DGS training."),
				TotalViews));
		}
		else if (TotalViews > 500)
		{
			OutWarnings.Add(FString::Printf(
				TEXT("High viewpoint count (%d). May significantly increase capture and training time."),
				TotalViews));
		}

		// Check radius
		if (Config.BaseRadius < 100.0f)
		{
			OutWarnings.Add(TEXT("Very small radius (<1m). May cause near-plane clipping issues."));
		}
		else if (Config.BaseRadius > 10000.0f)
		{
			OutWarnings.Add(TEXT("Very large radius (>100m). May affect depth precision."));
		}

		// Check elevation range
		if (Config.MaxElevation - Config.MinElevation < 30.0f)
		{
			OutWarnings.Add(TEXT("Narrow elevation range (<30°). May result in incomplete vertical coverage."));
		}

		// Check views per ring for overlap
		float AngularStep = 360.0f / Config.ViewsPerRing;
		if (AngularStep > 30.0f)
		{
			OutWarnings.Add(FString::Printf(
				TEXT("Angular step (%.1f°) may result in insufficient overlap with 90° FOV."),
				AngularStep));
		}

		// Check for custom trajectory
		if (Config.TrajectoryType == ECameraTrajectoryType::Custom && Config.CustomWaypoints.Num() < 3)
		{
			OutWarnings.Add(TEXT("Custom trajectory requires at least 3 waypoints."));
			bIsValid = false;
		}

		return bIsValid;
	}

	float FCameraTrajectoryGenerator::CalculateAverageOverlap(
		const TArray<FCameraViewpoint>& Viewpoints,
		float HorizontalFOV)
	{
		if (Viewpoints.Num() < 2)
		{
			return 0.0f;
		}

		float TotalOverlap = 0.0f;
		int32 OverlapCount = 0;

		// Calculate overlap between adjacent viewpoints
		for (int32 i = 0; i < Viewpoints.Num(); ++i)
		{
			int32 NextIdx = (i + 1) % Viewpoints.Num();

			// Calculate angular separation
			FVector Dir1 = Viewpoints[i].Rotation.Vector();
			FVector Dir2 = Viewpoints[NextIdx].Rotation.Vector();

			float AngleBetween = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Dir1, Dir2)));

			// Overlap = 1 - (angle / FOV), clamped to [0, 1]
			float Overlap = FMath::Clamp(1.0f - (AngleBetween / HorizontalFOV), 0.0f, 1.0f);

			TotalOverlap += Overlap;
			OverlapCount++;
		}

		return OverlapCount > 0 ? TotalOverlap / OverlapCount : 0.0f;
	}

	FRotator FCameraTrajectoryGenerator::CalculateLookAtRotation(
		const FVector& CameraPosition,
		const FVector& TargetPosition,
		float PitchOffset)
	{
		FVector Direction = (TargetPosition - CameraPosition).GetSafeNormal();
		FRotator LookAtRotation = Direction.Rotation();

		// Apply pitch offset
		LookAtRotation.Pitch += PitchOffset;

		return LookAtRotation;
	}

	FVector FCameraTrajectoryGenerator::SphericalToCartesian(
		float Radius,
		float ElevationDegrees,
		float AzimuthDegrees,
		const FVector& Center)
	{
		float ElevationRadians = FMath::DegreesToRadians(ElevationDegrees);
		float AzimuthRadians = FMath::DegreesToRadians(AzimuthDegrees);

		// UE5 coordinate system: X=Forward, Y=Right, Z=Up
		float CosElevation = FMath::Cos(ElevationRadians);

		FVector LocalPosition(
			Radius * CosElevation * FMath::Cos(AzimuthRadians),
			Radius * CosElevation * FMath::Sin(AzimuthRadians),
			Radius * FMath::Sin(ElevationRadians)
		);

		return Center + LocalPosition;
	}

	FVector FCameraTrajectoryGenerator::FibonacciSpherePoint(
		int32 Index,
		int32 TotalPoints,
		float Radius,
		const FVector& Center)
	{
		// Golden ratio for Fibonacci sphere
		const float GoldenRatio = (1.0f + FMath::Sqrt(5.0f)) / 2.0f;

		float Y = 1.0f - (static_cast<float>(Index) / (TotalPoints - 1)) * 2.0f; // -1 to 1
		float RadiusAtY = FMath::Sqrt(1.0f - Y * Y);

		float Theta = 2.0f * PI * Index / GoldenRatio;

		// Convert to UE5 coordinates
		FVector LocalPosition(
			RadiusAtY * FMath::Cos(Theta) * Radius,
			RadiusAtY * FMath::Sin(Theta) * Radius,
			Y * Radius
		);

		return Center + LocalPosition;
	}
}
