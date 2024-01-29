// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSerialization.h"

namespace SPP
{
	class SPP_CORE_API ZipBinarySerializer : public BinarySerializer
	{
	protected:
		struct Impl;
		std::unique_ptr<Impl> _impl;

	public:
		ZipBinarySerializer(bool bIsCompressing, const char* FileName);
		virtual ~ZipBinarySerializer();

		virtual void Seek(int64_t DataPos) override;
		virtual int64_t Tell() const override;
		virtual int64_t Size() const override;
		virtual bool Write(const void* Data, int64_t DataIn) override;
		virtual bool Read(void* Data, int64_t DataIn) override;
	};
}