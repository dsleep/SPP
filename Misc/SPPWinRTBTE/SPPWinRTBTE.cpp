// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPWinRTBTE.h"
#include "SPPLogging.h"
#include "SPPString.h"

#include <winrt/base.h>


#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/Windows.System.Threading.h>

#include <chrono>
#include "combaseapi.h"

#include <iostream>
#include <string>
#include <sstream> 
#include <iomanip>
#include <set>
#include <mutex>

SPP_OVERLOAD_ALLOCATORS

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Devices;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

using namespace std::chrono_literals;

IDispatcherQueueController controller{ nullptr };

static void ValidWinRT()
{
	static bool winRTStart = false;
	if (!winRTStart)
	{
		winrt::init_apartment();
		winRTStart = true;

		//controller = DispatcherQueueController::CreateOnDedicatedThread();
	}
}

namespace std
{
	template<> struct less<winrt::guid>
	{
		bool operator() (const winrt::guid& lhs, const winrt::guid& rhs) const
		{
			return memcmp(&lhs, &rhs, sizeof(winrt::guid)) < 0;
		}
	};
}


std::wstring guidToString(const winrt::guid& uuid)
{
	auto UUIStrign = to_hstring(uuid);	
	return std::wstring(UUIStrign);
}

std::wstring advertisementTypeToString(winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementType advertisementType)
{
	std::wstring ret;

	switch (advertisementType)
	{
	case winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementType::ConnectableUndirected:
		ret = L"ConnectableUndirected";
		break;
	case winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementType::ConnectableDirected:
		ret = L"ConnectableDirected";
		break;
	case winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementType::ScannableUndirected:
		ret = L"ScannableUndirected";
		break;
	case winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementType::NonConnectableUndirected:
		ret = L"NonConnectableUndirected";
		break;
	case winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementType::ScanResponse:
		ret = L"ScanResponse";
		break;
	default:
		break;
	}

	return ret;
}

std::wstring bluetoothAddressTypeToString(winrt::Windows::Devices::Bluetooth::BluetoothAddressType bluetoothAddressType)
{
	std::wstring ret;

	switch (bluetoothAddressType)
	{
	case winrt::Windows::Devices::Bluetooth::BluetoothAddressType::Public:
		ret = L"Public";
		break;
	case winrt::Windows::Devices::Bluetooth::BluetoothAddressType::Random:
		ret = L"Random";
		break;
	case winrt::Windows::Devices::Bluetooth::BluetoothAddressType::Unspecified:
		ret = L"Unspecified";
		break;
	default:
		break;
	}

	return ret;
}

namespace SPP
{
	LogEntry LOG_BTE("BTE");

	uint32_t GetWinRTBTWVersion()
	{
		return 1;
	}
				
	struct INTERNAL_BTEWatcher : winrt::implements<INTERNAL_BTEWatcher, IInspectable>
	{
	private:
		//winrt::Windows::Devices::Enumeration::DeviceWatcher _deviceWatcher{ nullptr };
		winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher _advertisemenWatcher{ nullptr };

		std::atomic_bool bDeviceFound{ 0 };
		std::atomic_bool bStarted{ 0 };

		std::atomic_bool _stopWatcherAction{ false };
		IAsyncAction _openDeviceAction{ nullptr };

		winrt::guid RequestedServiceGUID;
		std::map<winrt::guid, IBTEWatcher* > CharToFuncMap;
		
		winrt::event_token _deviceWatcherAddedToken;
		winrt::event_token _deviceWatcherUpdatedToken;
		winrt::event_token _deviceWatcherRemovedToken;
		winrt::event_token _deviceWatcherEnumerationCompletedToken;
		winrt::event_token _deviceWatcherStoppedToken;
		
		struct BTEData
		{
			std::string GUID;
			std::string Name;

			winrt::Windows::Devices::Bluetooth::BluetoothLEDevice device{ nullptr };
			
			std::vector<GattCharacteristic> readCharacteristic;
			std::map<std::string, GattCharacteristic> writeCharacteristics;

			bool Valid() const
			{
				return !GUID.empty();
			}

			~BTEData()
			{
				if (device)
				{
					device.Close();
				}
			}
		};

		std::mutex _devicesMutex;
		std::map< std::string, std::shared_ptr<BTEData> > _devices;

	public:
		INTERNAL_BTEWatcher()
		{		
		}

		IAsyncAction OpenDevice(unsigned long long deviceAddress)
		{
			BluetoothLEDevice device = co_await BluetoothLEDevice::FromBluetoothAddressAsync(deviceAddress);

			
			if (!device)
			{
				SPP_LOG(LOG_BTE, LOG_INFO, "OpenDevice failed 0x%X", deviceAddress);
				co_return;
			}

			auto gattSession = co_await GattSession::FromDeviceIdAsync(device.BluetoothDeviceId());
			gattSession.MaintainConnection(true);

			bool bIsValidDevice = false;
			auto sDeviceID = std::wstring_to_utf8(std::wstring(device.DeviceId()));
			auto sDeviceIDU = std::str_to_upper(sDeviceID);
			auto sDeviceName = std::wstring_to_utf8(std::wstring(device.Name()));
			
			std::vector<GattCharacteristic> readCharacteristic;

			//std::wcout << std::hex <<
			//	"\tDevice Information: " << std::endl <<
			//	"\tBluetoothAddress: [" << device.BluetoothAddress() << "]" << std::endl <<
			//	"\tBluetoothAddressType: [" << bluetoothAddressTypeToString(device.BluetoothAddressType()) << "]" << std::endl <<
			//	"\tConnectionStatus: [" << (device.ConnectionStatus() == BluetoothConnectionStatus::Connected ? "Connected" : "Disconnected") << "]" << std::endl <<
			//	"\tDeviceId: [" << device.DeviceId().c_str() << "]" << std::endl <<
			//	std::endl;
				
			winrt::Windows::Devices::Bluetooth::BluetoothCacheMode cacheMode = BluetoothCacheMode::Cached;

			SPP_LOG(LOG_BTE, LOG_INFO, "OpenDevice %s(0x%X)", sDeviceIDU.c_str(), deviceAddress);

			auto services = co_await device.GetGattServicesAsync(cacheMode);

			SPP_LOG(LOG_BTE, LOG_INFO, " - service count %d", services.Services().Size() );

			for (GenericAttributeProfile::GattDeviceService const& s : services.Services())
			{				
				auto sServiceID = std::wstring_to_utf8(guidToString(s.Uuid()));
				SPP_LOG(LOG_BTE, LOG_INFO, " - has Service %s", sServiceID.c_str());

				if (RequestedServiceGUID != s.Uuid())
				{
					SPP_LOG(LOG_BTE, LOG_INFO, " - ignoring");
					continue;
				}

				auto characteristics = co_await s.GetCharacteristicsAsync(cacheMode);

				for (GenericAttributeProfile::GattCharacteristic const& c : characteristics.Characteristics())
				{
					auto foundCharToWatch = CharToFuncMap.find(c.Uuid());
					if (foundCharToWatch == CharToFuncMap.end())
					{
						continue;
					}

					auto sCharID = std::wstring_to_utf8(guidToString(c.Uuid()));
					SPP_LOG(LOG_BTE, LOG_INFO, "   - has characteristic %s", sCharID.c_str());
					uint32_t charProps = (uint32_t) c.CharacteristicProperties();

					auto currentDescriptorValue = co_await c.ReadClientCharacteristicConfigurationDescriptorAsync();
					auto curValue = currentDescriptorValue.ClientCharacteristicConfigurationDescriptor();

					if ( charProps & (uint32_t)GattCharacteristicProperties::Read )
					{
						SPP_LOG(LOG_BTE, LOG_INFO, "   - is read characteristic");
					}					
					if (charProps & (uint32_t)GattCharacteristicProperties::Notify)
					{
						SPP_LOG(LOG_BTE, LOG_INFO, "   - is notify characteristic");
					}

					if (charProps & (uint32_t)GattCharacteristicProperties::Read)
					{
						auto readResult = co_await c.ReadValueAsync();
						if (readResult.Status() == GattCommunicationStatus::Success)
						{
							SPP_LOG(LOG_BTE, LOG_INFO, "Characteristic Data - Size: %d", readResult.Value().Length());
							//DataReader reader = DataReader::FromBuffer(readResult.Value());
							//SPP_LOG(LOG_BTE, LOG_INFO, "Characteristic Data - [" << reader.ReadString(readResult.Value().Length()).c_str() << "]" << std::endl;
						}
					}

					if (charProps & (uint32_t)GattCharacteristicProperties::Notify)
					{
						bIsValidDevice = true;
						readCharacteristic.push_back(c);

						auto descriptors = co_await c.GetDescriptorsAsync(cacheMode);
						SPP_LOG(LOG_BTE, LOG_INFO, "   - has %d descriptors", descriptors.Descriptors().Size() );
						for (GenericAttributeProfile::GattDescriptor const& d : descriptors.Descriptors())
						{
							auto sDescID = std::wstring_to_utf8(guidToString(d.Uuid()));
							SPP_LOG(LOG_BTE, LOG_INFO, "     - asking desc for notify: %s", sDescID.c_str());

							//c.set
							//GattCommunicationStatus statusC = co_await c.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify);
							GattCommunicationStatus statusC = co_await c.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify);

							auto writer = winrt::Windows::Storage::Streams::DataWriter();
							writer.WriteByte(0x01); //we want notifications
							writer.WriteByte(0x00);
							auto status = co_await d.WriteValueWithResultAsync(writer.DetachBuffer());
																				
							if (status.Status() == GattCommunicationStatus::Success)
							{
								SPP_LOG(LOG_BTE, LOG_INFO, "WRITE OK");
							}
							else
							{
								SPP_LOG(LOG_BTE, LOG_INFO, "WRITE FAILED");
							}
						}

						c.ValueChanged({ get_weak(), &INTERNAL_BTEWatcher::Characteristic_ValueChanged });

						//c.ValueChanged([](GattCharacteristic const& charateristic, GattValueChangedEventArgs const& args)
						//{
						//	SPP_LOG(LOG_BTE, LOG_INFO, "has value changed");
						//	//std::wcout << std::hex <<
						//	//	"\t\tNotified GattCharacteristic - Guid: [" << guidToString(charateristic.Uuid()) << "]" << std::endl;

						//	//DataReader reader = DataReader::FromBuffer(args.CharacteristicValue());

						//	//// Note this assumes value is string the characteristic type must be determined before reading
						//	//// This can display junk or maybe blowup
						//	//std::wcout << "\t\t\tCharacteristic Data - [" << reader.ReadString(args.CharacteristicValue().Length()).c_str() << "]" << std::endl;
						//});
					}
				}
			}

			if(bIsValidDevice)
			{
				std::unique_lock<std::mutex> lock(_devicesMutex);

				if (_devices.find(sDeviceIDU) == _devices.end())
				{
					std::shared_ptr< BTEData > thisDevice;
					thisDevice.reset(new BTEData{ sDeviceIDU, sDeviceName });
					thisDevice->device = device;
					thisDevice->readCharacteristic = std::move(readCharacteristic);

					device.GattServicesChanged({ get_weak(), &INTERNAL_BTEWatcher::BTEDevice_ServicesChanged });
					device.ConnectionStatusChanged({ get_weak(), &INTERNAL_BTEWatcher::BTEDevice_ConnectionStatusChanged });

					_devices[sDeviceIDU] = thisDevice;
				}
			}

			//device.Close();
		}


		void StartWatching(const std::string& DeviceData, const std::map< std::string, IBTEWatcher* >& CharacterFunMap)
		{
			if (bStarted)
			{
				SPP_LOG(LOG_BTE, LOG_INFO, "BTEWatcher::StartWatching already started");
			}

			CLSIDFromString(utf8_to_wstring(std::string_format("{%s}", DeviceData.c_str())).c_str(), (LPCLSID)&RequestedServiceGUID);

			SPP_LOG(LOG_BTE, LOG_INFO, "BTEWatcher::StartWatching %s", DeviceData.c_str());
				
			//winrt::guid newCharToWatch;
			//CLSIDFromString(L"{366DEE95-85A3-41C1-A507-8C3E02342001}", (LPCLSID)&newCharToWatch);

			for (auto& [key, Value] : CharacterFunMap)
			{
				winrt::guid newCharToWatch;
				CLSIDFromString(utf8_to_wstring(std::string_format("{%s}", key.c_str())).c_str(), (LPCLSID)&newCharToWatch);
				CharToFuncMap[newCharToWatch] = Value;
			}

			//bStarted = true;
			//_watcherAction = BTEControllerAction();

			_advertisemenWatcher = winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher();

			std::wcout << std::hex <<
				"BluetoothLEAdvertisementWatcher:" << std::endl <<
				"\tMaxOutOfRangeTimeout: [0x" << _advertisemenWatcher.MaxOutOfRangeTimeout().count() << "]" << std::endl <<
				"\tMaxSamplingInterval:  [0x" << _advertisemenWatcher.MaxSamplingInterval().count() << "]" << std::endl <<
				"\tMinOutOfRangeTimeout: [0x" << _advertisemenWatcher.MinOutOfRangeTimeout().count() << "]" << std::endl <<
				"\tMinSamplingInterval:  [0x" << _advertisemenWatcher.MinSamplingInterval().count() << "]" << std::endl <<
				std::endl;

			_advertisemenWatcher.ScanningMode(winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEScanningMode::Passive);

			_advertisemenWatcher.Received([&](winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher watcher,
				winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs eventArgs)
			{
				if (_openDeviceAction &&
					_openDeviceAction.Status() != AsyncStatus::Completed)
				{
					return;
				}

				if (IsConnected()) {
					return;
				}

				//watcher.Stop();

				auto sDeviceName = std::wstring_to_utf8(std::wstring(eventArgs.Advertisement().LocalName()));
				auto sType = std::wstring_to_utf8(advertisementTypeToString(eventArgs.AdvertisementType()));
				//SPP_LOG(LOG_BTE, LOG_INFO, "AdvertisementReceived: %s(%s)", sDeviceName.c_str(), sType.c_str());

				//std::wcout <<
				//	"AdvertisementReceived:" << std::endl <<
				//	"\tLocalName: [" << eventArgs.Advertisement().LocalName().c_str() << "]" <<
				//	"\tAdvertisementType: [" << advertisementTypeToString(eventArgs.AdvertisementType()) << "]" <<
				//	"\tBluetoothAddress: [0x" << std::hex << eventArgs.BluetoothAddress() << "]" <<
				//	"\tRawSignalStrengthInDBm: [" << std::dec << eventArgs.RawSignalStrengthInDBm() << "]" <<
				//	std::endl;

				for (auto const& g : eventArgs.Advertisement().ServiceUuids())
				{
					auto currentServiceGUID = std::wstring_to_utf8(guidToString(g));
					
					//std::wcout << "ServiceUUID: [" << guidToString(g) << "]" << std::endl;
					if (RequestedServiceGUID == g)
					{
						SPP_LOG(LOG_BTE, LOG_INFO, " - adv has service we need: %s", currentServiceGUID.c_str());

						auto deviceAddress = eventArgs.BluetoothAddress();
						//watcher.Stop();

						_openDeviceAction = OpenDevice(deviceAddress);

						break;
					}
				}

				//deviceAddress = eventArgs.BluetoothAddress();
			});

			std::cout << "Waiting for device: ";

			_advertisemenWatcher.Start();
		}

		IAsyncAction ProcessWrite(GattCharacteristic curChar, Buffer InBuffer)
		{
			co_await curChar.WriteValueWithResultAsync(InBuffer);
		}

		void WriteData(const std::string& DeviceID, 
			const std::string& WriteID,
			const void* buf, uint16_t BufferSize)
		{
			auto sDeviceID = std::utf8_to_wstring(DeviceID);

			GattCharacteristic writeChar{ nullptr };
			{
				std::unique_lock<std::mutex> lock(_devicesMutex);

				for (auto& [key, value] : _devices)
				{
					auto foundChar = value->writeCharacteristics.find(WriteID);
					if (foundChar != value->writeCharacteristics.end())
					{
						writeChar = foundChar->second;
					}
				}
			}

			if(writeChar)
			{
				Buffer dataBuff{ (uint32_t)BufferSize };
				memcpy(dataBuff.data(), buf, BufferSize);				
				ProcessWrite(writeChar, dataBuff);
			}
		}

		bool IsConnected()
		{
			std::unique_lock<std::mutex> lock(_devicesMutex);

			return _devices.empty() == false;
		}

		void Stop()
		{
			//if (_watcherAction)
			//{
			//	_watcherAction.Cancel();				
			//	_watcherAction = nullptr;
			//}

			//_deviceWatcher.Stop();
		}

		void Update()
		{
			// do stuff?	
		}

		void Characteristic_ValueChanged(GattCharacteristic const& changed, GattValueChangedEventArgs args)
		{
			//std::wcout << L"Characteristic_ValueChanged: " << std::endl;

			auto foundIt = CharToFuncMap.find(changed.Uuid());
			if (foundIt != CharToFuncMap.end())
			{
				auto iBuffer = args.CharacteristicValue();				
				foundIt->second->IncomingData(iBuffer.data(), iBuffer.Length());
			}
		}	

		void ClearDevice(winrt::Windows::Devices::Bluetooth::BluetoothLEDevice inDevice)
		{
			if (inDevice)
			{
				auto sDeviceID = std::wstring_to_utf8(std::wstring(inDevice.DeviceId()));
				auto sDeviceIDU = std::str_to_upper(sDeviceID);

				std::unique_lock<std::mutex> lock(_devicesMutex);

				auto foundDevice = _devices.find(sDeviceIDU);
				if (foundDevice != _devices.end())
				{
					SPP_LOG(LOG_BTE, LOG_INFO, "ClearDevice found: %s", sDeviceIDU.c_str());
					_devices.erase(foundDevice);
				}
				else
				{
					SPP_LOG(LOG_BTE, LOG_INFO, "ClearDevice: no device found %s", sDeviceIDU.c_str());
				}
			}
		}

		void BTEDevice_ServicesChanged(winrt::Windows::Devices::Bluetooth::BluetoothLEDevice inDevice, 
			winrt::Windows::Foundation::IInspectable deviceInfo)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "BTEDevice_ServicesChanged");			
			ClearDevice(inDevice);
		}

		void BTEDevice_ConnectionStatusChanged(winrt::Windows::Devices::Bluetooth::BluetoothLEDevice sender, winrt::Windows::Foundation::IInspectable deviceInfo)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "BTEDevice_ConnectionStatusChanged");
			ClearDevice(sender);
		}		
	};


	struct BTEWatcher::PlatImpl
	{
		winrt::com_ptr<INTERNAL_BTEWatcher> _watcher;
	};

	BTEWatcher::BTEWatcher() : _impl(new PlatImpl())
	{
		ValidWinRT();
		_impl->_watcher = winrt::make_self<INTERNAL_BTEWatcher>();
	}

	BTEWatcher::~BTEWatcher()
	{
	}

	void BTEWatcher::WatchForData(const std::string& DeviceData, const std::map< std::string, IBTEWatcher* >& CharacterFunMap)
	{
		_impl->_watcher->StartWatching(DeviceData, CharacterFunMap);
	}

	void BTEWatcher::WriteData(const std::string& DeviceData, const std::string& WriteID, const void* buf, uint16_t BufferSize)
	{
		_impl->_watcher->WriteData(DeviceData, WriteID, buf, BufferSize);
	}

	void BTEWatcher::Update() 
	{
		_impl->_watcher->Update();
	}

	bool BTEWatcher::IsConnected() const
	{
		return _impl->_watcher->IsConnected();
	}

	void BTEWatcher::Stop()
	{
		_impl->_watcher->Stop();
	}
}
