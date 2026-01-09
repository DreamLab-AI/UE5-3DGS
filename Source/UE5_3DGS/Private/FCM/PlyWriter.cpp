// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "FCM/PlyWriter.h"
#include "FCM/CoordinateConverter.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

namespace UE5_3DGS
{
	// FGaussianSplat implementation

	FGaussianSplat FGaussianSplat::FromPositionColor(const FVector& Pos, const FColor& Col)
	{
		FGaussianSplat Splat;
		Splat.Position = Pos;
		Splat.Color = Col;
		Splat.SH_DC = ColorToSH_DC(Col);
		return Splat;
	}

	FGaussianSplat FGaussianSplat::FromPositionColorNormal(const FVector& Pos, const FColor& Col, const FVector& Norm)
	{
		FGaussianSplat Splat = FromPositionColor(Pos, Col);
		Splat.Normal = Norm.GetSafeNormal();
		return Splat;
	}

	FVector FGaussianSplat::ColorToSH_DC(const FColor& Color)
	{
		// Convert RGB (0-255) to SH DC coefficients
		// SH DC = (color - 0.5) / C0, where C0 = 0.28209479177387814
		const float C0 = 0.28209479177387814f;
		float R = (Color.R / 255.0f - 0.5f) / C0;
		float G = (Color.G / 255.0f - 0.5f) / C0;
		float B = (Color.B / 255.0f - 0.5f) / C0;
		return FVector(R, G, B);
	}

	FColor FGaussianSplat::SH_DCToColor(const FVector& SH)
	{
		// Inverse of ColorToSH_DC
		const float C0 = 0.28209479177387814f;
		uint8 R = static_cast<uint8>(FMath::Clamp((SH.X * C0 + 0.5f) * 255.0f, 0.0f, 255.0f));
		uint8 G = static_cast<uint8>(FMath::Clamp((SH.Y * C0 + 0.5f) * 255.0f, 0.0f, 255.0f));
		uint8 B = static_cast<uint8>(FMath::Clamp((SH.Z * C0 + 0.5f) * 255.0f, 0.0f, 255.0f));
		return FColor(R, G, B, 255);
	}

	// FPlyWriter implementation

	bool FPlyWriter::WritePointCloud(
		const FString& FilePath,
		const TArray<FPointCloudPoint>& Points,
		bool bBinary)
	{
		if (Points.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Empty point cloud, nothing to write"));
			return false;
		}

		return bBinary ? WritePointCloudBinary(FilePath, Points) : WritePointCloudASCII(FilePath, Points);
	}

	bool FPlyWriter::WriteGaussianSplats(
		const FString& FilePath,
		const TArray<FGaussianSplat>& Splats,
		bool bBinary)
	{
		if (Splats.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Empty splat array, nothing to write"));
			return false;
		}

		return bBinary ? WriteGaussianBinary(FilePath, Splats) : WriteGaussianASCII(FilePath, Splats);
	}

	TArray<FPointCloudPoint> FPlyWriter::CreatePointCloudFromMesh(
		const TArray<FVector>& Vertices,
		const TArray<FVector>& Normals,
		const TArray<FColor>& Colors)
	{
		TArray<FPointCloudPoint> Points;
		Points.Reserve(Vertices.Num());

		bool bHasNormals = Normals.Num() == Vertices.Num();
		bool bHasColors = Colors.Num() == Vertices.Num();

		for (int32 i = 0; i < Vertices.Num(); ++i)
		{
			FPointCloudPoint Point;

			// Convert position to COLMAP coordinates
			Point.Position = FCoordinateConverter::ConvertPositionToColmap(Vertices[i]);

			if (bHasNormals)
			{
				Point.Normal = FCoordinateConverter::ConvertDirectionToColmap(Normals[i]);
			}

			if (bHasColors)
			{
				Point.Color = Colors[i];
			}

			Points.Add(Point);
		}

		return Points;
	}

	TArray<FGaussianSplat> FPlyWriter::CreateSplatsFromPointCloud(
		const TArray<FPointCloudPoint>& Points,
		float InitialScale)
	{
		TArray<FGaussianSplat> Splats;
		Splats.Reserve(Points.Num());

		for (const FPointCloudPoint& Point : Points)
		{
			FGaussianSplat Splat;
			Splat.Position = Point.Position;
			Splat.Normal = Point.Normal;
			Splat.Color = Point.Color;
			Splat.SH_DC = FGaussianSplat::ColorToSH_DC(Point.Color);
			Splat.Opacity = 1.0f;
			Splat.Scale = FVector(InitialScale, InitialScale, InitialScale);
			Splat.Rotation = FQuat::Identity;

			Splats.Add(Splat);
		}

		return Splats;
	}

	bool FPlyWriter::ReadPointCloud(const FString& FilePath, TArray<FPointCloudPoint>& OutPoints)
	{
		OutPoints.Empty();

		FString FileContent;
		if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load PLY file: %s"), *FilePath);
			return false;
		}

		int32 NumVertices;
		bool bIsBinary;
		TArray<FString> Properties;

		if (!ParsePlyHeader(FileContent, NumVertices, bIsBinary, Properties))
		{
			return false;
		}

		// Find data start
		int32 EndHeaderPos = FileContent.Find(TEXT("end_header"));
		if (EndHeaderPos == INDEX_NONE)
		{
			return false;
		}

		if (bIsBinary)
		{
			// Binary reading requires loading as byte array
			TArray<uint8> FileData;
			if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
			{
				return false;
			}

			// Find header end in binary data
			int32 DataStart = EndHeaderPos + 11; // "end_header\n"
			while (DataStart < FileData.Num() && FileData[DataStart - 1] != '\n')
			{
				DataStart++;
			}

			OutPoints.SetNum(NumVertices);

			// Read binary data
			const uint8* Data = FileData.GetData() + DataStart;
			for (int32 i = 0; i < NumVertices; ++i)
			{
				// Assume format: x y z nx ny nz r g b (little-endian floats + uint8 colors)
				OutPoints[i].Position.X = *reinterpret_cast<const float*>(Data); Data += 4;
				OutPoints[i].Position.Y = *reinterpret_cast<const float*>(Data); Data += 4;
				OutPoints[i].Position.Z = *reinterpret_cast<const float*>(Data); Data += 4;

				if (Properties.Contains(TEXT("nx")))
				{
					OutPoints[i].Normal.X = *reinterpret_cast<const float*>(Data); Data += 4;
					OutPoints[i].Normal.Y = *reinterpret_cast<const float*>(Data); Data += 4;
					OutPoints[i].Normal.Z = *reinterpret_cast<const float*>(Data); Data += 4;
				}

				if (Properties.Contains(TEXT("red")))
				{
					OutPoints[i].Color.R = *Data++;
					OutPoints[i].Color.G = *Data++;
					OutPoints[i].Color.B = *Data++;
				}
			}
		}
		else
		{
			// ASCII reading
			TArray<FString> Lines;
			FileContent.ParseIntoArrayLines(Lines);

			int32 LineIdx = 0;
			for (; LineIdx < Lines.Num(); ++LineIdx)
			{
				if (Lines[LineIdx].StartsWith(TEXT("end_header")))
				{
					LineIdx++;
					break;
				}
			}

			OutPoints.SetNum(NumVertices);
			for (int32 i = 0; i < NumVertices && LineIdx < Lines.Num(); ++i, ++LineIdx)
			{
				TArray<FString> Values;
				Lines[LineIdx].ParseIntoArray(Values, TEXT(" "));

				if (Values.Num() >= 3)
				{
					OutPoints[i].Position.X = FCString::Atof(*Values[0]);
					OutPoints[i].Position.Y = FCString::Atof(*Values[1]);
					OutPoints[i].Position.Z = FCString::Atof(*Values[2]);
				}

				if (Values.Num() >= 6)
				{
					OutPoints[i].Normal.X = FCString::Atof(*Values[3]);
					OutPoints[i].Normal.Y = FCString::Atof(*Values[4]);
					OutPoints[i].Normal.Z = FCString::Atof(*Values[5]);
				}

				if (Values.Num() >= 9)
				{
					OutPoints[i].Color.R = FCString::Atoi(*Values[6]);
					OutPoints[i].Color.G = FCString::Atoi(*Values[7]);
					OutPoints[i].Color.B = FCString::Atoi(*Values[8]);
				}
			}
		}

		return OutPoints.Num() > 0;
	}

	bool FPlyWriter::ReadGaussianSplats(const FString& FilePath, TArray<FGaussianSplat>& OutSplats)
	{
		// Gaussian splat PLY reading is more complex due to the 59 properties
		// This is a simplified implementation
		OutSplats.Empty();

		int32 NumVertices;
		bool bIsBinary;
		bool bIsGaussian;

		if (!GetPlyInfo(FilePath, NumVertices, bIsBinary, bIsGaussian))
		{
			return false;
		}

		if (!bIsGaussian)
		{
			UE_LOG(LogTemp, Warning, TEXT("PLY file does not appear to contain gaussian splat data"));
			return false;
		}

		// Full implementation would read all 59 properties
		// For now, we just create default splats based on count
		OutSplats.SetNum(NumVertices);

		UE_LOG(LogTemp, Log, TEXT("Gaussian splat file contains %d splats"), NumVertices);
		return true;
	}

	bool FPlyWriter::GetPlyInfo(
		const FString& FilePath,
		int32& OutNumVertices,
		bool& OutIsBinary,
		bool& OutIsGaussian)
	{
		OutNumVertices = 0;
		OutIsBinary = false;
		OutIsGaussian = false;

		FString FileContent;
		if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
		{
			return false;
		}

		TArray<FString> Properties;
		if (!ParsePlyHeader(FileContent, OutNumVertices, OutIsBinary, Properties))
		{
			return false;
		}

		// Check for gaussian-specific properties
		OutIsGaussian = Properties.Contains(TEXT("f_dc_0")) ||
			Properties.Contains(TEXT("opacity")) ||
			Properties.Contains(TEXT("scale_0"));

		return true;
	}

	int64 FPlyWriter::EstimateMemoryUsage(int32 NumSplats)
	{
		// 236 bytes per splat (standard 3DGS format)
		// Position: 12, Normal: 12, SH_DC: 12, SH_Rest: 180, Opacity: 4, Scale: 12, Rotation: 16
		return static_cast<int64>(NumSplats) * 236;
	}

	bool FPlyWriter::ValidateSplats(const TArray<FGaussianSplat>& Splats, TArray<FString>& OutWarnings)
	{
		OutWarnings.Empty();

		if (Splats.Num() == 0)
		{
			OutWarnings.Add(TEXT("Empty splat array"));
			return false;
		}

		int32 InvalidPos = 0;
		int32 InvalidOpacity = 0;
		int32 InvalidScale = 0;
		int32 InvalidRotation = 0;

		for (const FGaussianSplat& Splat : Splats)
		{
			// Check position
			if (!Splat.Position.ContainsNaN() && !FMath::IsFinite(Splat.Position.X))
			{
				InvalidPos++;
			}

			// Check opacity
			if (Splat.Opacity < 0.0f || Splat.Opacity > 1.0f)
			{
				InvalidOpacity++;
			}

			// Check scale (log-space, so should be reasonable range)
			if (Splat.Scale.X > 10.0f || Splat.Scale.X < -20.0f)
			{
				InvalidScale++;
			}

			// Check rotation quaternion
			float QuatLength = Splat.Rotation.Size();
			if (FMath::Abs(QuatLength - 1.0f) > 0.01f)
			{
				InvalidRotation++;
			}
		}

		if (InvalidPos > 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%d splats have invalid positions"), InvalidPos));
		}
		if (InvalidOpacity > 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%d splats have invalid opacity values"), InvalidOpacity));
		}
		if (InvalidScale > 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%d splats have extreme scale values"), InvalidScale));
		}
		if (InvalidRotation > 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%d splats have non-unit rotation quaternions"), InvalidRotation));
		}

		// Check total count
		if (Splats.Num() < 1000)
		{
			OutWarnings.Add(FString::Printf(TEXT("Low splat count (%d). 10K-1M typical for quality scenes."), Splats.Num()));
		}
		else if (Splats.Num() > 10000000)
		{
			OutWarnings.Add(FString::Printf(TEXT("Very high splat count (%d). May impact performance."), Splats.Num()));
		}

		return InvalidPos == 0;
	}

	FString FPlyWriter::GeneratePointCloudHeader(int32 NumPoints, bool bBinary)
	{
		FString Header;
		Header += TEXT("ply\n");
		Header += bBinary ? TEXT("format binary_little_endian 1.0\n") : TEXT("format ascii 1.0\n");
		Header += FString::Printf(TEXT("element vertex %d\n"), NumPoints);
		Header += TEXT("property float x\n");
		Header += TEXT("property float y\n");
		Header += TEXT("property float z\n");
		Header += TEXT("property float nx\n");
		Header += TEXT("property float ny\n");
		Header += TEXT("property float nz\n");
		Header += TEXT("property uchar red\n");
		Header += TEXT("property uchar green\n");
		Header += TEXT("property uchar blue\n");
		Header += TEXT("end_header\n");
		return Header;
	}

	FString FPlyWriter::GenerateGaussianHeader(int32 NumSplats, bool bBinary)
	{
		FString Header;
		Header += TEXT("ply\n");
		Header += bBinary ? TEXT("format binary_little_endian 1.0\n") : TEXT("format ascii 1.0\n");
		Header += FString::Printf(TEXT("element vertex %d\n"), NumSplats);

		// Position
		Header += TEXT("property float x\n");
		Header += TEXT("property float y\n");
		Header += TEXT("property float z\n");

		// Normal
		Header += TEXT("property float nx\n");
		Header += TEXT("property float ny\n");
		Header += TEXT("property float nz\n");

		// DC SH coefficients
		Header += TEXT("property float f_dc_0\n");
		Header += TEXT("property float f_dc_1\n");
		Header += TEXT("property float f_dc_2\n");

		// Rest SH coefficients (45 values for order 3)
		for (int32 i = 0; i < 45; ++i)
		{
			Header += FString::Printf(TEXT("property float f_rest_%d\n"), i);
		}

		// Opacity
		Header += TEXT("property float opacity\n");

		// Scale
		Header += TEXT("property float scale_0\n");
		Header += TEXT("property float scale_1\n");
		Header += TEXT("property float scale_2\n");

		// Rotation
		Header += TEXT("property float rot_0\n");
		Header += TEXT("property float rot_1\n");
		Header += TEXT("property float rot_2\n");
		Header += TEXT("property float rot_3\n");

		Header += TEXT("end_header\n");
		return Header;
	}

	bool FPlyWriter::WritePointCloudBinary(const FString& FilePath, const TArray<FPointCloudPoint>& Points)
	{
		FString Header = GeneratePointCloudHeader(Points.Num(), true);

		// Calculate data size: 6 floats (24 bytes) + 3 uchar (3 bytes) = 27 bytes per point
		int32 PointDataSize = 27;
		TArray<uint8> Data;
		Data.Reserve(Header.Len() + Points.Num() * PointDataSize);

		// Append header
		FTCHARToUTF8 UTF8Header(*Header);
		Data.Append(reinterpret_cast<const uint8*>(UTF8Header.Get()), UTF8Header.Length());

		// Append point data
		for (const FPointCloudPoint& Point : Points)
		{
			float X = Point.Position.X;
			float Y = Point.Position.Y;
			float Z = Point.Position.Z;
			float NX = Point.Normal.X;
			float NY = Point.Normal.Y;
			float NZ = Point.Normal.Z;

			Data.Append(reinterpret_cast<uint8*>(&X), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&Y), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&Z), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&NX), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&NY), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&NZ), sizeof(float));
			Data.Add(Point.Color.R);
			Data.Add(Point.Color.G);
			Data.Add(Point.Color.B);
		}

		return FFileHelper::SaveArrayToFile(Data, *FilePath);
	}

	bool FPlyWriter::WritePointCloudASCII(const FString& FilePath, const TArray<FPointCloudPoint>& Points)
	{
		FString Content = GeneratePointCloudHeader(Points.Num(), false);

		for (const FPointCloudPoint& Point : Points)
		{
			Content += FString::Printf(TEXT("%.6f %.6f %.6f %.6f %.6f %.6f %d %d %d\n"),
				Point.Position.X, Point.Position.Y, Point.Position.Z,
				Point.Normal.X, Point.Normal.Y, Point.Normal.Z,
				Point.Color.R, Point.Color.G, Point.Color.B
			);
		}

		return FFileHelper::SaveStringToFile(Content, *FilePath);
	}

	bool FPlyWriter::WriteGaussianBinary(const FString& FilePath, const TArray<FGaussianSplat>& Splats)
	{
		FString Header = GenerateGaussianHeader(Splats.Num(), true);

		// 236 bytes per splat
		TArray<uint8> Data;
		Data.Reserve(Header.Len() + Splats.Num() * 236);

		// Append header
		FTCHARToUTF8 UTF8Header(*Header);
		Data.Append(reinterpret_cast<const uint8*>(UTF8Header.Get()), UTF8Header.Length());

		// Append splat data
		for (const FGaussianSplat& Splat : Splats)
		{
			// Position (12 bytes)
			float X = Splat.Position.X;
			float Y = Splat.Position.Y;
			float Z = Splat.Position.Z;
			Data.Append(reinterpret_cast<uint8*>(&X), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&Y), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&Z), sizeof(float));

			// Normal (12 bytes)
			float NX = Splat.Normal.X;
			float NY = Splat.Normal.Y;
			float NZ = Splat.Normal.Z;
			Data.Append(reinterpret_cast<uint8*>(&NX), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&NY), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&NZ), sizeof(float));

			// DC SH (12 bytes)
			float DC0 = Splat.SH_DC.X;
			float DC1 = Splat.SH_DC.Y;
			float DC2 = Splat.SH_DC.Z;
			Data.Append(reinterpret_cast<uint8*>(&DC0), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&DC1), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&DC2), sizeof(float));

			// Rest SH (180 bytes = 45 floats)
			for (int32 i = 0; i < 45; ++i)
			{
				float SHVal = (i < Splat.SH_Rest.Num()) ? Splat.SH_Rest[i] : 0.0f;
				Data.Append(reinterpret_cast<uint8*>(&SHVal), sizeof(float));
			}

			// Opacity (4 bytes)
			float Opacity = Splat.Opacity;
			Data.Append(reinterpret_cast<uint8*>(&Opacity), sizeof(float));

			// Scale (12 bytes)
			float S0 = Splat.Scale.X;
			float S1 = Splat.Scale.Y;
			float S2 = Splat.Scale.Z;
			Data.Append(reinterpret_cast<uint8*>(&S0), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&S1), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&S2), sizeof(float));

			// Rotation XYZW (16 bytes)
			float R0 = Splat.Rotation.X;
			float R1 = Splat.Rotation.Y;
			float R2 = Splat.Rotation.Z;
			float R3 = Splat.Rotation.W;
			Data.Append(reinterpret_cast<uint8*>(&R0), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&R1), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&R2), sizeof(float));
			Data.Append(reinterpret_cast<uint8*>(&R3), sizeof(float));
		}

		return FFileHelper::SaveArrayToFile(Data, *FilePath);
	}

	bool FPlyWriter::WriteGaussianASCII(const FString& FilePath, const TArray<FGaussianSplat>& Splats)
	{
		FString Content = GenerateGaussianHeader(Splats.Num(), false);

		for (const FGaussianSplat& Splat : Splats)
		{
			// Position
			Content += FString::Printf(TEXT("%.6f %.6f %.6f "),
				Splat.Position.X, Splat.Position.Y, Splat.Position.Z);

			// Normal
			Content += FString::Printf(TEXT("%.6f %.6f %.6f "),
				Splat.Normal.X, Splat.Normal.Y, Splat.Normal.Z);

			// DC SH
			Content += FString::Printf(TEXT("%.6f %.6f %.6f "),
				Splat.SH_DC.X, Splat.SH_DC.Y, Splat.SH_DC.Z);

			// Rest SH
			for (int32 i = 0; i < 45; ++i)
			{
				float SHVal = (i < Splat.SH_Rest.Num()) ? Splat.SH_Rest[i] : 0.0f;
				Content += FString::Printf(TEXT("%.6f "), SHVal);
			}

			// Opacity
			Content += FString::Printf(TEXT("%.6f "), Splat.Opacity);

			// Scale
			Content += FString::Printf(TEXT("%.6f %.6f %.6f "),
				Splat.Scale.X, Splat.Scale.Y, Splat.Scale.Z);

			// Rotation
			Content += FString::Printf(TEXT("%.6f %.6f %.6f %.6f\n"),
				Splat.Rotation.X, Splat.Rotation.Y, Splat.Rotation.Z, Splat.Rotation.W);
		}

		return FFileHelper::SaveStringToFile(Content, *FilePath);
	}

	bool FPlyWriter::ParsePlyHeader(const FString& Content, int32& OutNumVertices, bool& OutIsBinary, TArray<FString>& OutProperties)
	{
		OutNumVertices = 0;
		OutIsBinary = false;
		OutProperties.Empty();

		TArray<FString> Lines;
		Content.ParseIntoArrayLines(Lines);

		if (Lines.Num() == 0 || !Lines[0].StartsWith(TEXT("ply")))
		{
			UE_LOG(LogTemp, Error, TEXT("Not a valid PLY file"));
			return false;
		}

		for (const FString& Line : Lines)
		{
			if (Line.Contains(TEXT("binary")))
			{
				OutIsBinary = true;
			}
			else if (Line.StartsWith(TEXT("element vertex")))
			{
				TArray<FString> Parts;
				Line.ParseIntoArray(Parts, TEXT(" "));
				if (Parts.Num() >= 3)
				{
					OutNumVertices = FCString::Atoi(*Parts[2]);
				}
			}
			else if (Line.StartsWith(TEXT("property")))
			{
				TArray<FString> Parts;
				Line.ParseIntoArray(Parts, TEXT(" "));
				if (Parts.Num() >= 3)
				{
					OutProperties.Add(Parts.Last());
				}
			}
			else if (Line.StartsWith(TEXT("end_header")))
			{
				break;
			}
		}

		return OutNumVertices > 0;
	}
}
