// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>
#include <vector>

#if _WIN32 && !defined(SPP_CRYPTO_STATIC)

	#ifdef SPP_CRYPTO_EXPORT
		#define SPP_CRYPTO_API __declspec(dllexport)
	#else
		#define SPP_CRYPTO_API __declspec(dllimport)
	#endif

#else
	
	#define SPP_CRYPTO_API 

#endif

namespace SPP
{
	class SPP_CRYPTO_API RSA_Cipher
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:
		RSA_Cipher();
		RSA_Cipher(const std::string &PublicKey);
		~RSA_Cipher();
		void GenerateKeyPair(uint32_t keyLength);

		void SetPublicKey(const std::string& PublicKey);
		std::string GetPublicKey() const;

		bool CanEncrypt() const;

		std::string EncryptString(const std::string &InString);
		std::string DecryptString(const std::string& InString);
	};

	class SPP_CRYPTO_API AES_Cipher
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:
		AES_Cipher();
		~AES_Cipher();
		void GenerateKey();
		std::string GetKey() const;
		void SetKey(const std::string& SharedKey);
		void EncryptData(const void *InData, size_t DataLength, std::vector<uint8_t>& oData);
		void DecryptData(const void* InData, size_t DataLength, std::vector<uint8_t> &oData);
	};

	SPP_CRYPTO_API std::string SHA256MemHash(const void* InMem, size_t MemSize);


}

