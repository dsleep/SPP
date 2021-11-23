// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "json/json.h"
#include "SPPLogging.h"
#include <set>

#include "SPPMemory.h"
#include "SPPPlatformCore.h"

#include "SPPCrypto.h"

using namespace SPP;

LogEntry LOG_APP("APP");

int main(int argc, char* argv[])
{
    IntializeCore(nullptr);

	// server
	RSA_Cipher rsaServer;
	rsaServer.GenerateKeyPair(1024);
	auto rsaPublicKey = rsaServer.GetPublicKey();

	SPP_LOG(LOG_APP, LOG_INFO, "rsaServer %S", rsaPublicKey.c_str());
	
	// client
	RSA_Cipher rsaClient;
	rsaClient.SetPublicKey(rsaPublicKey);
	AES_Cipher aesClient;
	aesClient.GenerateKey();
	auto symmetricKey = aesClient.GetKey();
	auto encryptedKey = rsaClient.EncryptString(symmetricKey);	

	SPP_LOG(LOG_APP, LOG_INFO, "aesClient %s", symmetricKey.c_str());
	SPP_LOG(LOG_APP, LOG_INFO, "aesClient encrypted %s", encryptedKey.c_str());

	// server again
	auto symmetricKeyCheck = rsaServer.DecryptString(encryptedKey);
	AES_Cipher aesServer;
	SE_ASSERT(symmetricKey == symmetricKeyCheck);
	aesServer.SetKey(symmetricKeyCheck);

	// data check
	std::vector<uint8_t> DataCheck;
	DataCheck.resize(500 * 1024);
	for (int32_t Iter = 0; Iter < DataCheck.size(); Iter++)
	{
		DataCheck[Iter] = (uint8_t)(Iter % 256);
	}	
	
	std::vector<uint8_t> encryptedData, decryptedData;	
	aesClient.EncryptData(DataCheck.data(), DataCheck.size(), encryptedData);	
	aesServer.DecryptData(encryptedData.data(), encryptedData.size(), decryptedData);

	SE_ASSERT(DataCheck == decryptedData);

    return 0;
}
