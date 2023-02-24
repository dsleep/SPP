// RemoteAppServer.cpp : Defines the entry point for the application.
//

#include "SPPCrypto.h"

#include "cryptopp/osrng.h"
#include "cryptopp/default.h"
#include "cryptopp/base64.h"
#include "cryptopp/aes.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/rsa.h"
#include "cryptopp/hex.h"
#include "cryptopp/randpool.h"
#include "cryptopp/sha.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	class VectorSink : public CryptoPP::Bufferless<CryptoPP::Sink>
	{
	public:

		VectorSink(std::vector<uint8_t>& out)
			: _out(&out) {
		}

		size_t Put2(const CryptoPP::byte* inString, size_t length, int /*messageEnd*/, bool /*blocking*/) 
		{
			_out->insert(_out->end(), inString, inString + length);
			return 0;
		}

	private:
		std::vector<uint8_t>* _out;
	};

	struct RSA_Cipher::PlatImpl
	{
		std::unique_ptr<CryptoPP::RSA::PrivateKey> privateKey;
		std::unique_ptr<CryptoPP::RSA::PublicKey> publicKey;
	};

	RSA_Cipher::RSA_Cipher() : _impl(new PlatImpl())
	{

	}
		
	RSA_Cipher::RSA_Cipher(const std::string& PublicKey) : _impl(new PlatImpl())
	{
		SetPublicKey(PublicKey);
	}

	RSA_Cipher::~RSA_Cipher()
	{

	}

	void RSA_Cipher::SetPublicKey(const std::string& PublicKey) 
	{
		CryptoPP::StringSource hexDecode(PublicKey, true, new CryptoPP::HexDecoder());
		_impl->publicKey.reset(new CryptoPP::RSA::PublicKey());
		_impl->publicKey->Load(hexDecode);
	}

	void RSA_Cipher::GenerateKeyPair(uint32_t keyLength)
	{
		CryptoPP::AutoSeededRandomPool rng;

		CryptoPP::InvertibleRSAFunction params;
		params.GenerateRandomWithKeySize(rng, 1024);

		_impl->privateKey.reset(new CryptoPP::RSA::PrivateKey(params));
		_impl->publicKey.reset(new CryptoPP::RSA::PublicKey(params));		
	}

	std::string RSA_Cipher::GetPublicKey() const
	{
		std::string PublicKey;
		CryptoPP::HexEncoder hexEncode(new CryptoPP::StringSink(PublicKey));
		_impl->publicKey->Save(hexEncode);
		return PublicKey;
	}

	bool RSA_Cipher::CanEncrypt() const
	{
		if (_impl->publicKey)
		{
			return true;
		}
		return false;
	}

	std::string RSA_Cipher::EncryptString(const std::string& InString)
	{
		CryptoPP::RSAES_OAEP_SHA_Encryptor pub( *_impl->publicKey );

		CryptoPP::AutoSeededRandomPool rng;

		std::string result;
		CryptoPP::StringSource(InString, true, new CryptoPP::PK_EncryptorFilter(rng,
			pub, 
			new CryptoPP::HexEncoder(new CryptoPP::StringSink(result))));
		return result;
	}
	std::string RSA_Cipher::DecryptString(const std::string& InString)
	{		
		CryptoPP::RSAES_OAEP_SHA_Decryptor priv(*_impl->privateKey);
		CryptoPP::AutoSeededRandomPool rng;

		std::string result;
		CryptoPP::StringSource(InString,
			true, 
			new CryptoPP::HexDecoder(new CryptoPP::PK_DecryptorFilter(rng, priv, new CryptoPP::StringSink(result))));
		return result;
	}


	struct AES_Cipher::PlatImpl
	{
		std::unique_ptr<CryptoPP::SecByteBlock> key;
		std::unique_ptr<CryptoPP::SecByteBlock> iv;
	};

	AES_Cipher::AES_Cipher() : _impl(new PlatImpl())
	{

	}
	AES_Cipher::~AES_Cipher()
	{
		
	}

	void AES_Cipher::GenerateKey()
	{
		//
		// Create AES keys
		//
		CryptoPP::AutoSeededRandomPool rng;		
		_impl->key.reset(new CryptoPP::SecByteBlock(CryptoPP::AES::DEFAULT_KEYLENGTH));
		_impl->iv.reset(new CryptoPP::SecByteBlock(CryptoPP::AES::BLOCKSIZE));
		rng.GenerateBlock(*_impl->key, _impl->key->size());
		rng.GenerateBlock(*_impl->iv, _impl->iv->size());
	}

	void AES_Cipher::SetKey(const std::string& SharedHexKey)
	{
		std::vector<uint8_t> keyAndIV;
		CryptoPP::HexDecoder hex(new CryptoPP::VectorSink(keyAndIV));
		hex.Put((CryptoPP::byte*)SharedHexKey.data(), SharedHexKey.length());
		hex.MessageEnd();

		_impl->key.reset(new CryptoPP::SecByteBlock(keyAndIV.data(), CryptoPP::AES::DEFAULT_KEYLENGTH));
		_impl->iv.reset(new CryptoPP::SecByteBlock(keyAndIV.data() + CryptoPP::AES::DEFAULT_KEYLENGTH, CryptoPP::AES::BLOCKSIZE));
	}

	std::string AES_Cipher::GetKey() const
	{
		std::string outString;
		CryptoPP::HexEncoder hex(new CryptoPP::StringSink(outString));
		hex.Put((CryptoPP::byte*)_impl->key->data(), _impl->key->size());
		hex.Put((CryptoPP::byte*)_impl->iv->data(), _impl->iv->size());
		hex.MessageEnd();
		return outString;
	}

	void AES_Cipher::EncryptData(const void* InData, size_t DataLength, std::vector<uint8_t>& oData)
	{
		if (_impl && _impl->key && _impl->iv)
		{
			CryptoPP::AES::Encryption aesEncryption(*_impl->key, CryptoPP::AES::DEFAULT_KEYLENGTH);
			CryptoPP::CBC_Mode_ExternalCipher::Encryption cbcEncryption(aesEncryption, *_impl->iv);

			CryptoPP::ArraySource s((const CryptoPP::byte*)InData, DataLength, true,
				new CryptoPP::StreamTransformationFilter(cbcEncryption,
					new CryptoPP::VectorSink(oData)
				) // StreamTransformationFilter
			); // StringSource
		}
	}

	void AES_Cipher::DecryptData(const void* InData, size_t DataLength, std::vector<uint8_t>& oData)
	{
		if (_impl && _impl->key && _impl->iv)
		{
			CryptoPP::AES::Decryption aesDecryption(*_impl->key, CryptoPP::AES::DEFAULT_KEYLENGTH);
			CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, *_impl->iv);

			CryptoPP::ArraySource sa((const CryptoPP::byte*)InData, DataLength, true,
				new CryptoPP::StreamTransformationFilter(cbcDecryption,
					new CryptoPP::VectorSink(oData)
				) // StreamTransformationFilter
			); // StringSource
		}
	}

	std::string SHA256MemHash(const void* InMem, size_t MemSize)
	{
		CryptoPP::SHA256 hash;
		std::string digest;

		hash.Update((const CryptoPP::byte*)InMem, MemSize);
		//digest.resize(hash.DigestSize());
		//hash.Final((CryptoPP::byte*)&digest[0]);
		digest.resize(hash.DigestSize() / 2);
		hash.TruncatedFinal((CryptoPP::byte*)&digest[0], digest.size());

		std::string oDigest;
		CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(oDigest));
		CryptoPP::StringSource(digest, true, new CryptoPP::Redirector(encoder));
		return oDigest;
	}
	
}
