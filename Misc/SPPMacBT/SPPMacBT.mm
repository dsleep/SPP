// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPMacBT.h"
#include "SPPLogging.h"
#include <string>

#include "SPPMacBTDelegate.h"

#import <dispatch/dispatch.h>

SPP::LogEntry LOG_MACBLE("MACBLE");

struct CharMap : public CWrapper
{
    std::map< std::string, SPP::IBTEWatcher* > data;
    virtual ~CharMap() {}
};

@implementation GenericCWrapper
-(void)dealloc {
    
    CWrapper* CData = (CWrapper*)[_data pointerValue];
    if(CData)
    {
        delete CData;
        CData = 0;
    }
    [_data release];
    _data = nil;
    [super dealloc];
}
-(instancetype)initCPtr:(NSValue*)InPtr
{
    _data = InPtr;
    return self;
}
-(NSValue *)getData
{
    return _data;
}
@end


@implementation BTEMonitor

@synthesize repeatingTimer;
@synthesize peripheralList;
@synthesize scanningUUID;
@synthesize charMapWrapper;

- (void)initialize
{
    NSLog(@"BTEMonitor: initialize");
    
    _stopWatcherAction = false;
    _BLECapable = false;
    scanningUUID = nil;
    
    peripheralList = [[NSMutableArray alloc] init];// [NSMutableArray array];
    manager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
}

/*
 - (void) applicationWillTerminate:(NSNotification *)notification
 {
 if(peripheral)
 {
 [manager cancelPeripheralConnection:peripheral];
 }
 }
 */

- (void) dealloc
{
    [self stopScan];
    
    [peripheral setDelegate:nil];
    [peripheral release];
    
    [peripheralList release];
    [manager release];
    
    [super dealloc];
}

#pragma mark - Start/Stop Scan methods

/*
 Uses CBCentralManager to check whether the current platform/hardware supports Bluetooth LE. An alert is raised if Bluetooth LE is not enabled or is not supported.
 */
- (BOOL) isLECapableHardware
{
    NSString * state = nil;
    
    switch ([manager state])
    {
        case CBManagerStateUnsupported:
            state = @"The platform/hardware doesn't support Bluetooth Low Energy.";
            break;
        case CBManagerStateUnauthorized:
            state = @"The app is not authorized to use Bluetooth Low Energy.";
            break;
        case CBManagerStatePoweredOff:
            state = @"Bluetooth is currently powered off.";
            break;
        case CBManagerStatePoweredOn:
            return TRUE;
        case CBManagerStateUnknown:
        default:
            return FALSE;
            
    }
    
    NSLog(@"Central manager state: %@", state);
    
    return FALSE;
}

-(void) BLEUpdate_Timer
{
    bool isScanning = [manager isScanning];
    bool bHaveActiveDevice = (peripheral != nil);
    
    if (bHaveActiveDevice)
    {
        if(isScanning)
        {
            [manager stopScan];
        }
    }
    
    if (scanningUUID != nil &&
        !bHaveActiveDevice &&
        !isScanning &&
        [self isLECapableHardware])
    {
        [manager scanForPeripheralsWithServices:@[scanningUUID] options:nil ];
    }
}

@class GenericCWrapper;
/*
 Request CBCentralManager to scan for heart rate peripherals using service UUID 0x180D
 */
- (void) startScan:(NSString *)InUUID CharMap:(GenericCWrapper*)InCharMap
{
    NSLog(@"starting scan");
    scanningUUID = [CBUUID UUIDWithString:InUUID];
    charMapWrapper = InCharMap;
    
    [scanningUUID retain];
    [charMapWrapper retain];
    
    // Cancel a preexisting timer.
    [self.repeatingTimer invalidate];
    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                                      target:self
                                                    selector:@selector(BLEUpdate_Timer)
                                                    userInfo: nil
                                                     repeats:YES];
    self.repeatingTimer = timer;
}

/*
 Request CBCentralManager to stop scanning for heart rate peripherals
 */
- (void) stopScan
{
    [self.repeatingTimer invalidate];
    [manager stopScan];
}

- (bool) IsConnected
{
    return (peripheral != nil);
}

- (void) writeData:(NSString *)InUUID DeviceID:(NSString*)InDeviceID WriteData:(NSData*)InWriteData
{
    if(peripheral && writeChars)
    {
        CBCharacteristic *foundChar = [writeChars objectForKey:InDeviceID];
        
        if(foundChar)
        {
            [peripheral writeValue:InWriteData forCharacteristic:foundChar type:CBCharacteristicWriteWithResponse];
        }
    }
}

#pragma mark - CBCentralManager delegate methods
/*
 Invoked whenever the central manager's state is updated.
 */
- (void) centralManagerDidUpdateState:(CBCentralManager *)central 
{
    _BLECapable = [self isLECapableHardware];
    NSLog(@"centralManagerDidUpdateState %d", _BLECapable);
}

/*
 Invoked when the central discovers heart rate peripheral while scanning.
 */
- (void) centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)aPeripheral advertisementData:(NSDictionary *)advertisementData RSSI:(NSNumber *)RSSI 
{
    NSLog(@"Found peripheral: %@ (%@)(%@)", aPeripheral.name, aPeripheral.identifier.UUIDString, aPeripheral.identifier.description);
    
    if( ![peripheralList containsObject:aPeripheral] )
    {
        [peripheralList addObject:aPeripheral];
    }
    
    if([peripheralList count] >=1)
    {
        [manager stopScan];
        peripheral = [peripheralList objectAtIndex:0];
        [peripheral retain];
        writeChars = [[NSMutableDictionary alloc] init];
        [manager connectPeripheral:peripheral options:[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES] forKey:CBConnectPeripheralOptionNotifyOnDisconnectionKey]];
    }
    
    /* Retreive already known devices */
    //[manager retrievePeripheralsWithIdentifiers:[NSArray arrayWithObject:(id)aPeripheral.identifier]];
}

/*
 Invoked when the central manager retrieves the list of known peripherals.
 Automatically connect to first known peripheral
 */
- (void)centralManager:(CBCentralManager *)central didRetrievePeripherals:(NSArray *)peripherals
{
    NSLog(@"Retrieved peripheral: %lu - %@", [peripherals count], peripherals);
    
    [manager stopScan];
    
    /* If there are any known devices, automatically connect to it.*/
    if([peripherals count] >=1)
    {
        peripheral = [peripherals objectAtIndex:0];
        [peripheral retain];
        writeChars = [[NSMutableDictionary alloc] init];
        [manager connectPeripheral:peripheral options:[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES] forKey:CBConnectPeripheralOptionNotifyOnDisconnectionKey]];
    }
}

/*
 Invoked whenever a connection is succesfully created with the peripheral.
 Discover available services on the peripheral
 */
- (void) centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)aPeripheral 
{    
    [aPeripheral setDelegate:self];
    [aPeripheral discoverServices:nil];
}

/*
 Invoked whenever an existing connection with the peripheral is torn down.
 Reset local variables
 */
- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)aPeripheral error:(NSError *)error
{
    if( peripheral )
    {
        [peripheral setDelegate:nil];
        [peripheral release];
        peripheral = nil;
    }
}

/*
 Invoked whenever the central manager fails to create a connection with the peripheral.
 */
- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)aPeripheral error:(NSError *)error
{
    NSLog(@"Fail to connect to peripheral: %@ with error = %@", aPeripheral, [error localizedDescription]);
    if( peripheral )
    {
        [peripheral setDelegate:nil];
        [peripheral release];
        peripheral = nil;
    }
}

#pragma mark - CBPeripheral delegate methods
/*
 Invoked upon completion of a -[discoverServices:] request.
 Discover available characteristics on interested services
 */
- (void) peripheral:(CBPeripheral *)aPeripheral didDiscoverServices:(NSError *)error 
{
    for (CBService *aService in aPeripheral.services)
    {
        NSLog(@"Found Service with UUID: %@", aService.UUID);
        // our initial requested service
        if ([aService.UUID isEqual:scanningUUID])
        {
            [aPeripheral discoverCharacteristics:nil forService:aService];
        }
    }
}

/*
 Invoked upon completion of a -[discoverCharacteristics:forService:] request.
 Perform appropriate operations on interested characteristics
 */
- (void) peripheral:(CBPeripheral *)aPeripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error 
{    
    for (CBCharacteristic *aChar in service.characteristics)
    {
        NSLog(@"Found Characteristic: %@", aChar.UUID);
                
        std::string CharUUID = std::string([aChar.UUID.UUIDString UTF8String]);                
        NSValue *charMap = [charMapWrapper getData];
        CharMap* CData = (CharMap*)[charMap pointerValue];
                        
        if(CData)
        {
            std::map< std::string, SPP::IBTEWatcher* > &CharToFuncMap = CData->data;
            
            //Check if the Characteristic is writable
            if ((aChar.properties & CBCharacteristicPropertyWrite) ||
                (aChar.properties & CBCharacteristicPropertyWriteWithoutResponse))
            {
                NSLog(@" - is a write char");
                [writeChars setValue:aChar forKey:aChar.UUID.UUIDString];
            }
            
            if (CharToFuncMap.find(CharUUID) == CharToFuncMap.end())
            {
                continue;
            }
            
            NSLog(@" - requested watch");
            [peripheral setNotifyValue:YES forCharacteristic:aChar];
        }
    }
}

/*
 Invoked upon completion of a -[readValueForCharacteristic:] request or on the reception of a notification/indication.
 */
- (void) peripheral:(CBPeripheral *)aPeripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error 
{
    std::string CharUUID = std::string([characteristic.UUID.UUIDString UTF8String]);
    
    NSValue *charMap = [charMapWrapper getData];
    CharMap* CData = (CharMap*)[charMap pointerValue];
    
    if(CData)
    {
        std::map< std::string, SPP::IBTEWatcher* > &CharToFuncMap = CData->data;
        auto foundValue = CharToFuncMap.find(CharUUID);
        
        if (foundValue != CharToFuncMap.end())
        {
            NSData * updatedValue = characteristic.value;
            uint8_t* dataPointer = (uint8_t*)[updatedValue bytes];
            
            foundValue->second->IncomingData(dataPointer, updatedValue.length);
        }
    }
}

- (void) peripheral:(CBPeripheral *)peripheral didModifyServices:(NSArray<CBService *> *)invalidatedServices
{
    if( peripheral )
    {
        [peripheral setDelegate:nil];
        
        [manager cancelPeripheralConnection:peripheral];
        
        [peripheral release];
        peripheral = nil;
    }
}


@end



namespace SPP
{
SPP_CORE_API LogEntry LOG_MACBT("MACBT");




uint32_t GetMacBTWVersion()
{
    return 1;
}

struct BTEWatcher::PlatImpl
{
    BTEMonitor *_watcher = nullptr;
};

BTEWatcher::BTEWatcher() : _impl(new PlatImpl())
{
    dispatch_async(dispatch_get_main_queue(), ^{
        _impl->_watcher = [[BTEMonitor alloc] init];
        [_impl->_watcher initialize];
    });
}

BTEWatcher::~BTEWatcher()
{
    if(_impl->_watcher)
    {
        [_impl->_watcher release];
        _impl->_watcher = nil;
    }
}

struct __attribute__((objc_boxable)) B_CharactersticMap
{
    std::map< std::string, IBTEWatcher* >  dataChunk;
};

typedef struct __attribute__((objc_boxable)) std::map< std::string, IBTEWatcher* > _boxInterfaceMap;

void BTEWatcher::WatchForData(const std::string& DeviceData, const std::map< std::string, IBTEWatcher* >& CharacterFunMap)
{
    SPP_LOG(LOG_MACBT, LOG_INFO, "BTEWatcher::WatchForData: %s", DeviceData.c_str());
    NSString *nsDevice = [NSString stringWithUTF8String:DeviceData.c_str()];
    
    CharMap *newMap = new CharMap();
    newMap->data = CharacterFunMap;
    NSValue* value = [NSValue valueWithPointer: newMap];
    GenericCWrapper *InWrapper = [[GenericCWrapper alloc] initCPtr:value];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [_impl->_watcher startScan:nsDevice CharMap:InWrapper];
    });
}

void BTEWatcher::WriteData(const std::string& DeviceData, const std::string& WriteID, const void* buf, uint16_t BufferSize)
{
    NSData* valData = [NSData dataWithBytes:(void*)buf length:BufferSize];
    NSString *nsDevice = [NSString stringWithUTF8String:DeviceData.c_str()];
    NSString *serviceID = [NSString stringWithUTF8String:WriteID.c_str()];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [_impl->_watcher writeData:nsDevice DeviceID:serviceID WriteData:valData];
    });
}

void BTEWatcher::Update()
{
    // _impl->_watcher->Update();
}

void BTEWatcher::Stop()
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [_impl->_watcher stopScan];
    });
}

bool BTEWatcher::IsConnected()
{
    return [_impl->_watcher IsConnected];
}
}
