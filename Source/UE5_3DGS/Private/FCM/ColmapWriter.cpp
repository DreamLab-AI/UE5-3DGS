// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "FCM/ColmapWriter.h"
#include "FCM/CoordinateConverter.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

namespace UE5_3DGS
{
	bool FColmapWriter::WriteColmapDataset(
		const FString& OutputDir,
		const TArray<FColmapCamera>& Cameras,
		const TArray<FColmapImage>& Images,
		const TArray<FColmapPoint3D>& Points3D,
		bool bBinary)
	{
		// Create directory structure
		if (!CreateDirectoryStructure(OutputDir))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create COLMAP directory structure"));
			return false;
		}

		FString SparseDir = OutputDir / TEXT("sparse") / TEXT("0");
		FString Extension = bBinary ? TEXT(".bin") : TEXT(".txt");

		// Write cameras
		FString CamerasPath = SparseDir / (TEXT("cameras") + Extension);
		if (!WriteCameras(CamerasPath, Cameras, bBinary))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to write cameras file"));
			return false;
		}

		// Write images
		FString ImagesPath = SparseDir / (TEXT("images") + Extension);
		if (!WriteImages(ImagesPath, Images, bBinary))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to write images file"));
			return false;
		}

		// Write points3D
		FString Points3DPath = SparseDir / (TEXT("points3D") + Extension);
		if (!WritePoints3D(Points3DPath, Points3D, bBinary))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to write points3D file"));
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("COLMAP dataset written successfully to: %s"), *OutputDir);
		return true;
	}

	bool FColmapWriter::WriteCameras(
		const FString& FilePath,
		const TArray<FColmapCamera>& Cameras,
		bool bBinary)
	{
		return bBinary ? WriteCamerasBinary(FilePath, Cameras) : WriteCamerasText(FilePath, Cameras);
	}

	bool FColmapWriter::WriteImages(
		const FString& FilePath,
		const TArray<FColmapImage>& Images,
		bool bBinary)
	{
		return bBinary ? WriteImagesBinary(FilePath, Images) : WriteImagesText(FilePath, Images);
	}

	bool FColmapWriter::WritePoints3D(
		const FString& FilePath,
		const TArray<FColmapPoint3D>& Points,
		bool bBinary)
	{
		return bBinary ? WritePoints3DBinary(FilePath, Points) : WritePoints3DText(FilePath, Points);
	}

	bool FColmapWriter::WriteCamerasText(const FString& FilePath, const TArray<FColmapCamera>& Cameras)
	{
		FString Content;

		// Header comment
		Content += TEXT("# Camera list with one line of data per camera:\n");
		Content += TEXT("#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]\n");
		Content += TEXT("# Number of cameras: ") + FString::FromInt(Cameras.Num()) + TEXT("\n");

		for (const FColmapCamera& Cam : Cameras)
		{
			// Format: CAMERA_ID MODEL WIDTH HEIGHT PARAMS[]
			Content += FString::Printf(TEXT("%d %s %d %d %s\n"),
				Cam.CameraId,
				*Cam.Intrinsics.GetColmapModelName(),
				Cam.Intrinsics.Width,
				Cam.Intrinsics.Height,
				*Cam.Intrinsics.GetColmapParamsString()
			);
		}

		return FFileHelper::SaveStringToFile(Content, *FilePath);
	}

	bool FColmapWriter::WriteImagesText(const FString& FilePath, const TArray<FColmapImage>& Images)
	{
		FString Content;

		// Header comment
		Content += TEXT("# Image list with two lines of data per image:\n");
		Content += TEXT("#   IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n");
		Content += TEXT("#   POINTS2D[] as (X, Y, POINT3D_ID)\n");
		Content += TEXT("# Number of images: ") + FString::FromInt(Images.Num()) + TEXT("\n");

		for (const FColmapImage& Img : Images)
		{
			// Line 1: IMAGE_ID QW QX QY QZ TX TY TZ CAMERA_ID NAME
			// COLMAP quaternion order is (w, x, y, z)
			Content += FString::Printf(TEXT("%d %.10f %.10f %.10f %.10f %.10f %.10f %.10f %d %s\n"),
				Img.ImageId,
				Img.Rotation.W, Img.Rotation.X, Img.Rotation.Y, Img.Rotation.Z,
				Img.Translation.X, Img.Translation.Y, Img.Translation.Z,
				Img.CameraId,
				*Img.ImageName
			);

			// Line 2: POINTS2D (empty for initial capture, populated after feature matching)
			if (Img.Keypoints.Num() > 0)
			{
				FString Keypoints;
				for (int32 i = 0; i < Img.Keypoints.Num(); ++i)
				{
					Keypoints += FString::Printf(TEXT("%.2f %.2f -1 "),
						Img.Keypoints[i].X, Img.Keypoints[i].Y);
				}
				Content += Keypoints.TrimEnd() + TEXT("\n");
			}
			else
			{
				Content += TEXT("\n"); // Empty keypoints line
			}
		}

		return FFileHelper::SaveStringToFile(Content, *FilePath);
	}

	bool FColmapWriter::WritePoints3DText(const FString& FilePath, const TArray<FColmapPoint3D>& Points)
	{
		FString Content;

		// Header comment
		Content += TEXT("# 3D point list with one line of data per point:\n");
		Content += TEXT("#   POINT3D_ID, X, Y, Z, R, G, B, ERROR, TRACK[] as (IMAGE_ID, POINT2D_IDX)\n");
		Content += TEXT("# Number of points: ") + FString::FromInt(Points.Num()) + TEXT("\n");

		for (const FColmapPoint3D& Pt : Points)
		{
			// POINT3D_ID X Y Z R G B ERROR TRACK[]
			FString Track;
			for (int32 i = 0; i < Pt.ImageIds.Num() && i < Pt.Point2DIndices.Num(); ++i)
			{
				Track += FString::Printf(TEXT("%d %d "), Pt.ImageIds[i], Pt.Point2DIndices[i]);
			}

			Content += FString::Printf(TEXT("%lld %.10f %.10f %.10f %d %d %d %.6f %s\n"),
				Pt.PointId,
				Pt.Position.X, Pt.Position.Y, Pt.Position.Z,
				Pt.Color.R, Pt.Color.G, Pt.Color.B,
				Pt.Error,
				*Track.TrimEnd()
			);
		}

		return FFileHelper::SaveStringToFile(Content, *FilePath);
	}

	bool FColmapWriter::WriteCamerasBinary(const FString& FilePath, const TArray<FColmapCamera>& Cameras)
	{
		TArray<uint8> Data;

		// Number of cameras (uint64)
		uint64 NumCameras = Cameras.Num();
		Data.Append(reinterpret_cast<uint8*>(&NumCameras), sizeof(uint64));

		for (const FColmapCamera& Cam : Cameras)
		{
			// Camera ID (uint32)
			uint32 CameraId = Cam.CameraId;
			Data.Append(reinterpret_cast<uint8*>(&CameraId), sizeof(uint32));

			// Model ID (int32)
			int32 ModelId = Cam.Intrinsics.GetColmapModelId();
			Data.Append(reinterpret_cast<uint8*>(&ModelId), sizeof(int32));

			// Width (uint64)
			uint64 Width = Cam.Intrinsics.Width;
			Data.Append(reinterpret_cast<uint8*>(&Width), sizeof(uint64));

			// Height (uint64)
			uint64 Height = Cam.Intrinsics.Height;
			Data.Append(reinterpret_cast<uint8*>(&Height), sizeof(uint64));

			// Parameters (double[])
			// Number of parameters depends on camera model
			TArray<double> Params;
			switch (Cam.Intrinsics.CameraModel)
			{
			case EColmapCameraModel::SIMPLE_PINHOLE:
				Params = { Cam.Intrinsics.FocalLengthX, Cam.Intrinsics.PrincipalPointX, Cam.Intrinsics.PrincipalPointY };
				break;
			case EColmapCameraModel::PINHOLE:
				Params = { Cam.Intrinsics.FocalLengthX, Cam.Intrinsics.FocalLengthY, Cam.Intrinsics.PrincipalPointX, Cam.Intrinsics.PrincipalPointY };
				break;
			case EColmapCameraModel::OPENCV:
				Params = { Cam.Intrinsics.FocalLengthX, Cam.Intrinsics.FocalLengthY, Cam.Intrinsics.PrincipalPointX, Cam.Intrinsics.PrincipalPointY,
					Cam.Intrinsics.K1, Cam.Intrinsics.K2, Cam.Intrinsics.P1, Cam.Intrinsics.P2 };
				break;
			default:
				Params = { Cam.Intrinsics.FocalLengthX, Cam.Intrinsics.FocalLengthY, Cam.Intrinsics.PrincipalPointX, Cam.Intrinsics.PrincipalPointY };
			}

			for (double Param : Params)
			{
				Data.Append(reinterpret_cast<uint8*>(&Param), sizeof(double));
			}
		}

		return FFileHelper::SaveArrayToFile(Data, *FilePath);
	}

	bool FColmapWriter::WriteImagesBinary(const FString& FilePath, const TArray<FColmapImage>& Images)
	{
		TArray<uint8> Data;

		// Number of images (uint64)
		uint64 NumImages = Images.Num();
		Data.Append(reinterpret_cast<uint8*>(&NumImages), sizeof(uint64));

		for (const FColmapImage& Img : Images)
		{
			// Image ID (uint32)
			uint32 ImageId = Img.ImageId;
			Data.Append(reinterpret_cast<uint8*>(&ImageId), sizeof(uint32));

			// Quaternion WXYZ (double[4])
			double QW = Img.Rotation.W;
			double QX = Img.Rotation.X;
			double QY = Img.Rotation.Y;
			double QZ = Img.Rotation.Z;
			Data.Append(reinterpret_cast<uint8*>(&QW), sizeof(double));
			Data.Append(reinterpret_cast<uint8*>(&QX), sizeof(double));
			Data.Append(reinterpret_cast<uint8*>(&QY), sizeof(double));
			Data.Append(reinterpret_cast<uint8*>(&QZ), sizeof(double));

			// Translation (double[3])
			double TX = Img.Translation.X;
			double TY = Img.Translation.Y;
			double TZ = Img.Translation.Z;
			Data.Append(reinterpret_cast<uint8*>(&TX), sizeof(double));
			Data.Append(reinterpret_cast<uint8*>(&TY), sizeof(double));
			Data.Append(reinterpret_cast<uint8*>(&TZ), sizeof(double));

			// Camera ID (uint32)
			uint32 CameraId = Img.CameraId;
			Data.Append(reinterpret_cast<uint8*>(&CameraId), sizeof(uint32));

			// Image name (null-terminated string)
			FTCHARToUTF8 UTF8Name(*Img.ImageName);
			Data.Append(reinterpret_cast<const uint8*>(UTF8Name.Get()), UTF8Name.Length() + 1);

			// Number of 2D points (uint64)
			uint64 NumPoints2D = Img.Keypoints.Num();
			Data.Append(reinterpret_cast<uint8*>(&NumPoints2D), sizeof(uint64));

			// 2D points (x, y, point3D_id) for each
			for (const FVector2D& KP : Img.Keypoints)
			{
				double X = KP.X;
				double Y = KP.Y;
				uint64 Point3DId = -1; // -1 = unmatched
				Data.Append(reinterpret_cast<uint8*>(&X), sizeof(double));
				Data.Append(reinterpret_cast<uint8*>(&Y), sizeof(double));
				Data.Append(reinterpret_cast<uint8*>(&Point3DId), sizeof(uint64));
			}
		}

		return FFileHelper::SaveArrayToFile(Data, *FilePath);
	}

	bool FColmapWriter::WritePoints3DBinary(const FString& FilePath, const TArray<FColmapPoint3D>& Points)
	{
		TArray<uint8> Data;

		// Number of points (uint64)
		uint64 NumPoints = Points.Num();
		Data.Append(reinterpret_cast<uint8*>(&NumPoints), sizeof(uint64));

		for (const FColmapPoint3D& Pt : Points)
		{
			// Point ID (uint64)
			uint64 PointId = Pt.PointId;
			Data.Append(reinterpret_cast<uint8*>(&PointId), sizeof(uint64));

			// Position XYZ (double[3])
			double X = Pt.Position.X;
			double Y = Pt.Position.Y;
			double Z = Pt.Position.Z;
			Data.Append(reinterpret_cast<uint8*>(&X), sizeof(double));
			Data.Append(reinterpret_cast<uint8*>(&Y), sizeof(double));
			Data.Append(reinterpret_cast<uint8*>(&Z), sizeof(double));

			// Color RGB (uint8[3])
			Data.Add(Pt.Color.R);
			Data.Add(Pt.Color.G);
			Data.Add(Pt.Color.B);

			// Error (double)
			double Error = Pt.Error;
			Data.Append(reinterpret_cast<uint8*>(&Error), sizeof(double));

			// Track length (uint64)
			uint64 TrackLen = FMath::Min(Pt.ImageIds.Num(), Pt.Point2DIndices.Num());
			Data.Append(reinterpret_cast<uint8*>(&TrackLen), sizeof(uint64));

			// Track entries (image_id, point2d_idx)
			for (int32 i = 0; i < static_cast<int32>(TrackLen); ++i)
			{
				uint32 ImgId = Pt.ImageIds[i];
				uint32 Pt2DIdx = Pt.Point2DIndices[i];
				Data.Append(reinterpret_cast<uint8*>(&ImgId), sizeof(uint32));
				Data.Append(reinterpret_cast<uint8*>(&Pt2DIdx), sizeof(uint32));
			}
		}

		return FFileHelper::SaveArrayToFile(Data, *FilePath);
	}

	FColmapCamera FColmapWriter::CreateCamera(
		const FCameraIntrinsics& Intrinsics,
		int32 CameraId)
	{
		FColmapCamera Camera;
		Camera.CameraId = CameraId;
		Camera.Intrinsics = Intrinsics;
		Camera.bIsShared = true;
		Camera.Model = Intrinsics.GetColmapModelName();
		Camera.Width = Intrinsics.Width;
		Camera.Height = Intrinsics.Height;
		Camera.Params = Intrinsics.GetColmapParamsString();
		return Camera;
	}

	TArray<FColmapImage> FColmapWriter::CreateImagesFromViewpoints(
		const TArray<FCameraViewpoint>& Viewpoints,
		const FCameraIntrinsics& Intrinsics,
		const FString& ImagePrefix,
		const FString& ImageExtension)
	{
		TArray<FColmapImage> Images;
		Images.Reserve(Viewpoints.Num());

		for (int32 i = 0; i < Viewpoints.Num(); ++i)
		{
			const FCameraViewpoint& VP = Viewpoints[i];

			FColmapImage Img;
			Img.ImageId = i + 1; // COLMAP IDs start at 1
			Img.CameraId = 1; // Shared camera model

			// Generate image filename
			Img.ImageName = ImagePrefix + FormatImageIndex(i) + ImageExtension;

			// Convert UE5 transform to COLMAP format
			FVector ColmapPos;
			FQuat ColmapRot;
			FCoordinateConverter::ConvertCameraToColmap(VP.GetTransform(), ColmapPos, ColmapRot);

			// COLMAP stores world-to-camera transform
			// Rotation is already world-to-camera from ConvertCameraToColmap
			Img.Rotation = ColmapRot;

			// Translation in COLMAP is -R * C where C is camera center
			// We already have world-to-camera, so translation is R * (-C) = -R * C
			FMatrix RotMatrix = FRotationMatrix::Make(ColmapRot);
			Img.Translation = RotMatrix.TransformVector(-ColmapPos);

			Images.Add(Img);
		}

		return Images;
	}

	bool FColmapWriter::CreateDirectoryStructure(const FString& OutputDir)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		// Create base directories
		TArray<FString> Dirs = {
			OutputDir,
			OutputDir / TEXT("sparse"),
			OutputDir / TEXT("sparse") / TEXT("0"),
			OutputDir / TEXT("images"),
			OutputDir / TEXT("depth") // For depth maps if exported
		};

		for (const FString& Dir : Dirs)
		{
			if (!PlatformFile.DirectoryExists(*Dir))
			{
				if (!PlatformFile.CreateDirectoryTree(*Dir))
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to create directory: %s"), *Dir);
					return false;
				}
			}
		}

		return true;
	}

	bool FColmapWriter::ValidateDataset(const FString& OutputDir, TArray<FString>& OutWarnings)
	{
		OutWarnings.Empty();
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		FString SparseDir = OutputDir / TEXT("sparse") / TEXT("0");

		// Check for required files
		TArray<FString> RequiredFiles = {
			SparseDir / TEXT("cameras.txt"),
			SparseDir / TEXT("cameras.bin"),
			SparseDir / TEXT("images.txt"),
			SparseDir / TEXT("images.bin")
		};

		bool bHasCameras = false;
		bool bHasImages = false;

		if (PlatformFile.FileExists(*(SparseDir / TEXT("cameras.txt"))) ||
			PlatformFile.FileExists(*(SparseDir / TEXT("cameras.bin"))))
		{
			bHasCameras = true;
		}

		if (PlatformFile.FileExists(*(SparseDir / TEXT("images.txt"))) ||
			PlatformFile.FileExists(*(SparseDir / TEXT("images.bin"))))
		{
			bHasImages = true;
		}

		if (!bHasCameras)
		{
			OutWarnings.Add(TEXT("Missing cameras file (cameras.txt or cameras.bin)"));
		}

		if (!bHasImages)
		{
			OutWarnings.Add(TEXT("Missing images file (images.txt or images.bin)"));
		}

		// Check images directory
		FString ImagesDir = OutputDir / TEXT("images");
		if (!PlatformFile.DirectoryExists(*ImagesDir))
		{
			OutWarnings.Add(TEXT("Images directory does not exist"));
		}
		else
		{
			// Count images
			TArray<FString> ImageFiles;
			PlatformFile.FindFiles(ImageFiles, *ImagesDir, TEXT("*.jpg"));
			PlatformFile.FindFiles(ImageFiles, *ImagesDir, TEXT("*.png"));

			if (ImageFiles.Num() == 0)
			{
				OutWarnings.Add(TEXT("No image files found in images directory"));
			}
			else if (ImageFiles.Num() < 50)
			{
				OutWarnings.Add(FString::Printf(TEXT("Low image count (%d). 100+ recommended for quality training."), ImageFiles.Num()));
			}
		}

		return bHasCameras && bHasImages;
	}

	FString FColmapWriter::FormatImageIndex(int32 Index, int32 NumDigits)
	{
		return FString::Printf(TEXT("%0*d"), NumDigits, Index);
	}
}
