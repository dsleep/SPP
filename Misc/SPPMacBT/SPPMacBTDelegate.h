// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <QuartzCore/QuartzCore.h>
#include <thread>
#include <memory>


struct CWrapper
{
    virtual ~CWrapper() {}
};

@interface GenericCWrapper : NSObject {
    NSValue *_data; //as CWrapper
}
-(void)dealloc;
-(NSValue *)getData;
-(instancetype)initCPtr:(NSValue*)InPtr;
@end

@class GenericCWrapper;



@interface BTEMonitor : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
{
    NSMutableArray *peripheralList;
    CBCentralManager *manager;
    
    // our active device
    CBPeripheral *peripheral;
    NSMutableDictionary *writeChars;
    
    bool _stopWatcherAction;// = false;
    bool _BLECapable;// = false;
    
    std::thread _worker;
}

@property (assign) NSTimer *repeatingTimer;
@property (retain) NSMutableArray *peripheralList;

@property (retain) CBUUID *scanningUUID;
@property (retain) GenericCWrapper *charMapWrapper;

- (void) initialize;
- (void) startScan:(NSString *)InUUID CharMap:(GenericCWrapper*)InCharMap;
- (void) writeData:(NSString *)InUUID DeviceID:(NSString*)InDeviceID WriteData:(NSData*)InWriteData;
- (bool) IsConnected;
- (void) stopScan;
- (BOOL) isLECapableHardware;
- (BOOL) BLEUpdate_Timer;

@end
