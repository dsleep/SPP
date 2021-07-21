// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPAssetCache.h"
#include "SPPFileSystem.h"

namespace SPP
{
	std::shared_ptr<BinaryBlobSerializer> GetCachedAsset(const AssetPath& AssetPath, const std::string Tag)
	{
		auto relativePath = AssetPath.GetRelativePath();
		auto CachePath = std::filesystem::path(GAssetPath) / "CACHE" / (relativePath + Tag + ".BIN");

		if (std::filesystem::exists(CachePath))
		{
			std::filesystem::file_time_type assetTime = std::filesystem::last_write_time(*AssetPath);
			std::filesystem::file_time_type cacheTime = std::filesystem::last_write_time(CachePath.generic_string().c_str());

			if (cacheTime >= assetTime)
			{
				std::vector<uint8_t> fileData;
				if (LoadFileToArray(CachePath.generic_string().c_str(), fileData))
				{
					return std::make_shared<BinaryBlobSerializer>(fileData);
				}
			}
		}	

		return nullptr;
	}
	
	void PutCachedAsset(const AssetPath& AssetPath, const BinaryBlobSerializer &InBinaryBlob, const std::string Tag)
	{
		auto relativePath = AssetPath.GetRelativePath();
		auto CachePath = std::filesystem::path(GAssetPath) / "CACHE" / (relativePath + Tag + ".BIN");
		auto parentPath = CachePath.parent_path();

		std::filesystem::create_directories(parentPath);

		WriteArrayToFile(CachePath.generic_string().c_str(), InBinaryBlob.GetArray());
	}
}