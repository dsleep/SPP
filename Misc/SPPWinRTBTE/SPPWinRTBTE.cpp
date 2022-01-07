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

		controller = DispatcherQueueController::CreateOnDedicatedThread();
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
		winrt::Windows::Devices::Enumeration::DeviceWatcher _deviceWatcher{ nullptr };

		std::atomic_bool bDeviceFound{ 0 };
		std::atomic_bool bStarted{ 0 };

		std::atomic_bool _stopWatcherAction{ false };
		IAsyncAction _watcherAction{ nullptr };

		winrt::guid RequestedServiceGUID;
		std::map<winrt::guid, IBTEWatcher* > CharToFuncMap;
		
		winrt::event_token _deviceWatcherAddedToken;
		winrt::event_token _deviceWatcherUpdatedToken;
		winrt::event_token _deviceWatcherRemovedToken;
		winrt::event_token _deviceWatcherEnumerationCompletedToken;
		winrt::event_token _deviceWatcherStoppedToken;
		
		struct BTEData
		{
			hstring GUID;
			hstring name;
			bool bNeedsUpdate{ true };
			std::atomic_bool bIsUpdating{ false };

			winrt::Windows::Devices::Bluetooth::BluetoothLEDevice device{ nullptr };
			
			std::vector<GattCharacteristic> readCharacteristic;
			std::vector<GattCharacteristic> writeCharacteristics;

			bool Valid() const
			{
				return !name.empty();
			}
		};

		std::mutex _devicesMutex;
		std::map< hstring, std::shared_ptr<BTEData> > _devices;

	public:
		INTERNAL_BTEWatcher()
		{				
			auto requestedProperties = single_threaded_vector<hstring>({
				L"System.Devices.Aep.DeviceAddress",
				L"System.Devices.Aep.IsConnected",
				L"System.Devices.Aep.Bluetooth.Le.IsConnectable" });

			// BT_Code: Example showing paired and non-paired in a single query.
			std::wstring aqsAllBluetoothLEDevices = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";

			_deviceWatcher = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
				aqsAllBluetoothLEDevices,
				requestedProperties,
				DeviceInformationKind::AssociationEndpoint);
			
			_deviceWatcherAddedToken = _deviceWatcher.Added({ get_weak(), &INTERNAL_BTEWatcher::DeviceWatcher_Added });
			_deviceWatcherUpdatedToken = _deviceWatcher.Updated({ get_weak(), &INTERNAL_BTEWatcher::DeviceWatcher_Updated });
			_deviceWatcherRemovedToken = _deviceWatcher.Removed({ get_weak(), &INTERNAL_BTEWatcher::DeviceWatcher_Removed });
			_deviceWatcherEnumerationCompletedToken = _deviceWatcher.EnumerationCompleted({ get_weak(), &INTERNAL_BTEWatcher::DeviceWatcher_EnumerationCompleted });
			_deviceWatcherStoppedToken = _deviceWatcher.Stopped({ get_weak(), &INTERNAL_BTEWatcher::DeviceWatcher_Stopped });
		}

		IAsyncAction BTEControllerAction()
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "BTEWatcher WatcherAction");

			_deviceWatcher.Start();

			while (!_stopWatcherAction)
			{		
				co_await 2s;

				bool bAnyValid = false;
				{
					std::unique_lock<std::mutex> lock(_devicesMutex);
					for (auto& [key, value] : _devices)
					{
						if (value->Valid())
						{
							bAnyValid = true;
							break;
						}
					}
				}

				auto curStatus = _deviceWatcher.Status();

				if (bAnyValid)
				{
					if (curStatus == DeviceWatcherStatus::Started)
					{
						_deviceWatcher.Stop();

						while (curStatus != DeviceWatcherStatus::Stopped)
						{
							curStatus = _deviceWatcher.Status();
							co_await 16ms;
						}
					}
				}
				else
				{
					if (curStatus == DeviceWatcherStatus::EnumerationCompleted)
					{
						_deviceWatcher.Stop();

						while (curStatus != DeviceWatcherStatus::Stopped)
						{
							curStatus = _deviceWatcher.Status();
							co_await 16ms;
						}

						_deviceWatcher.Start();
						continue;
					}
				}

				{
					std::map< hstring, std::shared_ptr<BTEData> > devicesCopy;
					
					{
						std::unique_lock<std::mutex> lock(_devicesMutex);
						devicesCopy = _devices;
					}

					for (auto& [key, value] : devicesCopy)
					{
						if (value->Valid() && value->bNeedsUpdate)
						{
							co_await UpdateDevice(value);
						}
					}
				}
			
			}
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

			bStarted = true;
			_watcherAction = BTEControllerAction();
		}

		IAsyncAction ProcessWrite(GattCharacteristic curChar, Buffer InBuffer)
		{
			co_await curChar.WriteValueWithResultAsync(InBuffer);
		}

		void WriteData(const std::string& DeviceID, const void* buf, uint16_t BufferSize)
		{
			auto sDeviceID = std::utf8_to_wstring(DeviceID);
			hstring asHString(sDeviceID);

			GattCharacteristic writeChar{ nullptr };
			{
				std::unique_lock<std::mutex> lock(_devicesMutex);
				auto foundDevice = _devices.find(asHString);
				if (foundDevice != _devices.end())
				{
					//TODO just front aint great
					writeChar = foundDevice->second->writeCharacteristics.front();
				}
			}

			if(writeChar)
			{
				Buffer dataBuff{ (uint32_t)BufferSize };
				memcpy(dataBuff.data(), buf, BufferSize);				
				ProcessWrite(writeChar, dataBuff);
			}
		}

		void Stop()
		{
			if (_watcherAction)
			{
				_watcherAction.Cancel();				
				_watcherAction = nullptr;
			}

			_deviceWatcher.Stop();
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
				
		void DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation deviceInfo)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "DeviceWatcher_Added: %s", std::wstring_to_utf8(std::wstring(deviceInfo.Id() + deviceInfo.Name())).c_str());

			auto DeviceName = deviceInfo.Name();
			auto sDeviceName = std::wstring_to_utf8(std::wstring(DeviceName));
			{
				std::unique_lock<std::mutex> lock(_devicesMutex);

				if (_devices.find(deviceInfo.Id()) == _devices.end())
				{
					std::shared_ptr< BTEData > newData;
					newData.reset(new BTEData{ deviceInfo.Id(), DeviceName });
					_devices[deviceInfo.Id()] = newData;
				}
			}
		}

		void BTEDevice_ServicesChanged(winrt::Windows::Devices::Bluetooth::BluetoothLEDevice inDevice, winrt::Windows::Foundation::IInspectable deviceInfo)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "BTEDevice_StatusChanged");

			if (inDevice)
			{
				std::unique_lock<std::mutex> lock(_devicesMutex);
				for (auto& [key, value] : _devices)
				{
					if (value->GUID == inDevice.DeviceId())
					{
						value->bNeedsUpdate = true;
						return;
					}
				}
			}
		}

		void BTEDevice_ConnectionStatusChanged(winrt::Windows::Devices::Bluetooth::BluetoothLEDevice sender, winrt::Windows::Foundation::IInspectable deviceInfo)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "BTEDevice_ConnectionStatusChanged");
		}

		IAsyncAction UpdateDevice(std::shared_ptr<BTEData> InDevice)
		{
			auto bCanUpdate = InDevice->bIsUpdating.exchange(true);
			if (bCanUpdate)
			{
				co_return;
			}

			//co_await winrt::resume_foreground(controller.DispatcherQueue());

			auto deviceID = InDevice->GUID;

			if (!InDevice->device)
			{
				try
				{
					// BT_Code: BluetoothLEDevice.FromIdAsync must be called from a UI thread because it may prompt for consent.
					InDevice->device = co_await BluetoothLEDevice::FromIdAsync(deviceID);

					if (InDevice->device == nullptr)
					{
						SPP_LOG(LOG_BTE, LOG_INFO, "Failed to connect to device");
					}
					else
					{
						InDevice->device.GattServicesChanged({ get_weak(), &INTERNAL_BTEWatcher::BTEDevice_ServicesChanged });
						InDevice->device.ConnectionStatusChanged({ get_weak(), &INTERNAL_BTEWatcher::BTEDevice_ConnectionStatusChanged });
					}					
				}
				catch (hresult_error& ex)
				{
					SPP_LOG(LOG_BTE, LOG_INFO, "Bluetooth radio is not on.");

				}
			}
			
			if (InDevice->device)
			{
				// Note: BluetoothLEDevice.GattServices property will return an empty list for unpaired devices. For all uses we recommend using the GetGattServicesAsync method.
				// BT_Code: GetGattServicesAsync returns a list of all the supported services of the device (even if it's not paired to the system).
				// If the services supported by the device are expected to change during BT usage, subscribe to the GattServicesChanged event.
				GattDeviceServicesResult result = co_await InDevice->device.GetGattServicesAsync(BluetoothCacheMode::Uncached);

				if (result.Status() == GattCommunicationStatus::Success)
				{
					IVectorView<GattDeviceService> services = result.Services();
					//rootPage.NotifyUser(L"Found " + to_hstring(services.Size()) + L" services", NotifyType::StatusMessage);
					for (auto&& service : services)
					{
						guid uuid = service.Uuid();
						auto UUIStrign = to_hstring(uuid);
						SPP_LOG(LOG_BTE, LOG_INFO, " - Service: %s", std::wstring_to_utf8(std::wstring(UUIStrign)).c_str());

						if (RequestedServiceGUID != uuid)
						{
							continue;
						}

						SPP_LOG(LOG_BTE, LOG_INFO, " *** FOUND Service *** DEVICE: %s", std::wstring_to_utf8(std::wstring(deviceID)).c_str());

						IVectorView<GattCharacteristic> characteristics{ nullptr };
						try
						{
							// Ensure we have access to the device.
							auto accessStatus = co_await service.RequestAccessAsync();
							if (accessStatus == DeviceAccessStatus::Allowed)
							{
								// BT_Code: Get all the child characteristics of a service. Use the cache mode to specify uncached characterstics only 
								// and the new Async functions to get the characteristics of unpaired devices as well. 
								GattCharacteristicsResult result = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
								if (result.Status() == GattCommunicationStatus::Success)
								{
									characteristics = result.Characteristics();
								}
								else
								{
									SPP_LOG(LOG_BTE, LOG_INFO, "Error accessing service: Gatt Failure");
								}
							}
							else
							{
								SPP_LOG(LOG_BTE, LOG_INFO, "Error accessing service");
							}
						}
						catch (hresult_error& ex)
						{
							SPP_LOG(LOG_BTE, LOG_INFO, "Restricted service. Cant read characteristics:");
						}

						if (characteristics)
						{
							InDevice->readCharacteristic.clear();
							InDevice->writeCharacteristics.clear();
							InDevice->bNeedsUpdate = false;

							for (GattCharacteristic&& c : characteristics)
							{
								guid uuid = c.Uuid();
								auto UUIStrign = to_hstring(uuid);
								SPP_LOG(LOG_BTE, LOG_INFO, " - Characteristic: %s", std::wstring_to_utf8(std::wstring(UUIStrign)).c_str());

								auto curProperties = c.CharacteristicProperties();
								if (((uint32_t)curProperties & (uint32_t)GattCharacteristicProperties::Write) != 0)
								{
									SPP_LOG(LOG_BTE, LOG_INFO, " - Write Prop");
									InDevice->writeCharacteristics.push_back(c);
								}
								else
								{
									SPP_LOG(LOG_BTE, LOG_INFO, " - Read Prop");
									InDevice->readCharacteristic.push_back(c);
								}

								if (CharToFuncMap.find(uuid) == CharToFuncMap.end())
								{
									continue;
								}

								GattClientCharacteristicConfigurationDescriptorValue cccdValue = GattClientCharacteristicConfigurationDescriptorValue::None;
								if ((c.CharacteristicProperties() & GattCharacteristicProperties::Indicate) != GattCharacteristicProperties::None)
								{
									cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
								}
								else if ((c.CharacteristicProperties() & GattCharacteristicProperties::Notify) != GattCharacteristicProperties::None)
								{
									cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Notify;
								}

								try
								{
									// BT_Code: Must write the CCCD in order for server to send indications.
									// We receive them in the ValueChanged event handler.
									GattCommunicationStatus status = co_await c.WriteClientCharacteristicConfigurationDescriptorAsync(cccdValue);

									if (status == GattCommunicationStatus::Success)
									{
										auto foundIt = CharToFuncMap.find(uuid);
										if (foundIt != CharToFuncMap.end())
										{
											foundIt->second->StateChange(EBTEState::Connected);
										}

										c.ValueChanged({ get_weak(), &INTERNAL_BTEWatcher::Characteristic_ValueChanged });

										SPP_LOG(LOG_BTE, LOG_INFO, "Successfully subscribed for value change");
									}
									else
									{
										SPP_LOG(LOG_BTE, LOG_INFO, "Error registering for value changes");
									}
								}
								catch (hresult_access_denied& ex)
								{
									SPP_LOG(LOG_BTE, LOG_INFO, "Error registering for value changes: hresult_access_denied");
								}
							}
						}
					}
				}
				else
				{
					SPP_LOG(LOG_BTE, LOG_INFO, "Device unreachable");
				}
			}

			InDevice->bIsUpdating = false;

			co_return;
		}

		void DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate deviceInfo)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "DeviceWatcher_Updated %s", std::wstring_to_utf8(std::wstring(deviceInfo.Id())).c_str());

			{
				std::unique_lock<std::mutex> lock(_devicesMutex);
				auto foundDevice = _devices.find(deviceInfo.Id());
				if (foundDevice == _devices.end() || !foundDevice->second->Valid())
				{
					return;
				}

				foundDevice->second->bNeedsUpdate = true;
			}	
		}

		void DeviceWatcher_Removed(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "DeviceWatcher_Removed %s", std::wstring_to_utf8(std::wstring(deviceInfoUpdate.Id())).c_str());
		}

		void DeviceWatcher_EnumerationCompleted(DeviceWatcher sender, IInspectable const&)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "DeviceWatcher_EnumerationCompleted");
		}

		void DeviceWatcher_Stopped(DeviceWatcher sender, IInspectable const&)
		{
			SPP_LOG(LOG_BTE, LOG_INFO, "DeviceWatcher_Stopped");
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

	void BTEWatcher::WriteData(const std::string& DeviceData, const void* buf, uint16_t BufferSize)
	{
		_impl->_watcher->WriteData(DeviceData, buf, BufferSize);
	}

	void BTEWatcher::Update() 
	{
		_impl->_watcher->Update();
	}

	void BTEWatcher::Stop()
	{
		_impl->_watcher->Stop();
	}
}
