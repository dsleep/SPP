// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPEngine.h"
#include "SPPFileSystem.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	SPP_ENGINE_API std::string GRootPath;
	SPP_ENGINE_API std::string GAssetPath;
	SPP_ENGINE_API std::string GBinaryPath;

	AssetPath::AssetPath(const stdfs::path& InAssetPath) 
	{
		if (InAssetPath.is_absolute())
		{
			stdfs::path mainAssetDir(GAssetPath);
			_relPath = stdfs::relative(InAssetPath, mainAssetDir);
		}
		else
		{
			_relPath = InAssetPath;
		}
	}

	AssetPath::AssetPath(const char* InAssetPath) : AssetPath(stdfs::path(InAssetPath)) {}
	AssetPath::AssetPath(const std::string& InAssetPath) : AssetPath(stdfs::path(InAssetPath)) {}

	const char* AssetPath::operator *() const
	{
		stdfs::path finalPath = GAssetPath;
		finalPath += _relPath;

		//not a big fan of this, good alternatives?
		const_cast<AssetPath*>(this)->_tmpFinalPathRet = stdfs::absolute(finalPath).generic_string();
		return _tmpFinalPathRet.c_str();
	}

	std::string AssetPath::GetExtension() const
	{
		return _relPath.extension().generic_string();
	}

	std::string AssetPath::GetName() const
	{
		return _relPath.filename().generic_string();
	}

	std::string AssetPath::GetRelativePath() const
	{
		return _relPath.generic_string();
	}

	stdfs::path AssetPath::GetAbsolutePath() const
	{
		stdfs::path finalPath = GAssetPath;
		finalPath += _relPath;
		return stdfs::absolute(finalPath);
	}
}
